/* 
 * $Id: notify.c,v 1.22 2002/10/17 20:25:56 ljb Exp $
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <string.h>
#include <sys/types.h>
#include <regex.h>
#include <unistd.h>
#include <time.h>
#include <irr_notify.h>
#include <pgp.h>

extern char *test_addr;

/* '\0' terminated set of email addrs.
 * Theses addrs are used to notify users
 * that authorised changes have occured
 * to their objects.
 */
char notify_addrs[MAX_ADDR_SIZE];
char *nnext;
int nndx[MAX_NDX_SIZE];
int num_notify;

/* '\0' terminated set of email addrs.
 * These addrs are used to notify users of
 * unauthorised updates.
 */
char forward_addrs[MAX_ADDR_SIZE];
char *fnext;
int fndx[MAX_NDX_SIZE];
int num_forward;

/* For uniform treatment, even though
 * there is only one sender, make it
 * like a forward_addr or notify_addr case
 */
char sender_addrs[MAX_ADDR_SIZE];
char *snext;
int sndx[1];
int num_sender;

char buf[MAXLINE];
/* local yokel's */
static void perform_rpsdist_trans (trace_t *, FILE *, ret_info_t *,
				   int, char *, long, char *IRRd_HOST, 
				   int IRRd_PORT, char *dbpdir);
static void perform_transactions (trace_t *, FILE *, ret_info_t *, int, 
				  char *, int, char *);
/* static void perform_transactions_new (trace_t *, FILE *, ret_info_t *, int, 
				  char *, int, char *); */
static void remove_tempfiles (trace_t *);
static FILE *rpsdist_fopen (char *);
static char *send_rps_dist_trans (trace_t *, FILE *, ret_info_t *, char *, 
				  char *, long, char *, int);
static char *init_jentries (trace_t *, char *, long, char *, FILE *);
static char *init_irrd_updates (trace_t *, FILE *, FILE *, ret_info_t *, const char *);
static char *init_pgp_updates (trace_t *, FILE *, ret_info_t *, char *, int *);
static long get_irrd_cs (trace_t *, char *, char *, int);
static void pgp_files_backout (char *);
static char *rpsdist_timestamp (char *);

/* Control the action of performing the udpates and sending
 * out notifications.  Briefly, for auth failures all referenced
 * upd_to: fields will be notified.  For error-free updates (ie,
 * no syntax and no auth errors) notifications will be sent to
 * all referenced notify: and mnt_nfy: field addresses.  The 
 * sender is always notified unless the '-x' command line flag is set
 * (see below).
 *
 * notify () will send the updates to IRRd.  If IRRd encounters
 * an error and/or cannot apply the update the IRRd error message 
 * will be relayed in the notification.
 *
 * 'null_notification' = 1 causes notify () to skip
 * all notifications.  In this case, notify () will send the
 * updates to IRRd but no notifications will be sent. '*tmpfname'
 * is the file template for 'mkstemp ()' to use to build
 * the notification files.  There will be one notification file
 * per unique notify/updto email address.
 *
 * 'null_submission' means the user sent in an empty submission.
 * For an email submission all we have is the email header and an
 * empty body.  For a TCP submission all we have is an empty body
 * and a return IP address.  Note that 'null_submission' would
 * have been better name 'non_null_lines' since 'null_submission'
 * equal to 0 means we have a NULL submission.
 *
 * Return:
 *   void
 *   Results of the updates are included in the notifications.
 */
void notify (trace_t *tr, char *tmpfname, FILE *fin, 
	     int null_submission, int null_notification, int dump_stdout,
	     char *web_origin_str, int rps_dist_flag, char *IRRd_HOST, int IRRd_PORT, 
	     char *db_admin, char *pgpdir, char *dbdir, char *tlogfn,
	     long tlog_fpos, FILE *ack_fd, char * from_ip) {

  ret_info_t rstart;
  trans_info_t trans_info;
  char buf[MAXLINE];
  irrd_result_t *p;
  long offset = 0;
  char pbuf[256];

  /*fprintf (dfile, "Enter notify()\n");*/

  rstart.first = rstart.last = NULL;

  if (rps_dist_flag)
    perform_rpsdist_trans (tr, fin, &rstart, null_submission, 
			   tlogfn, tlog_fpos, IRRd_HOST, IRRd_PORT, dbdir); 
  else
    perform_transactions (tr, fin, &rstart, null_submission, 
			  IRRd_HOST, IRRd_PORT, pgpdir);
    /* JW want to go to this when we make irr_submit/IRRd transaction compliant
    perform_transactions_new (tr, fin, &rstart, null_submission, 
			      IRRd_HOST, IRRd_PORT, pgpdir);
    */

  p = rstart.first;

  /* just to be safe rewind */
  if (fseek (fin, 0L, SEEK_SET) != 0) {
    fprintf (stderr, "ERROR: cannot rewind input file, exit!\n");
    exit (0);
  }

  /* init global data structures */
  nnext = notify_addrs;
  fnext = forward_addrs;
  snext = sender_addrs;
  num_notify = num_forward = num_sender = 0;
  /* pointer to array of file names and handles to 
   * notify/forward mail replies.
   */
  num_hdls = 0; 

  while (fgets (buf, MAXLINE, fin) != NULL) {
    if (strncmp (HDR_START, buf, strlen (HDR_START) - 1))
      continue;

    /* fprintf (dfile, "found a start header %s strlen-(%d)\n", buf, strlen (buf));*/
    
    if (parse_header (tr, fin, &offset, &trans_info))
      break; /* illegal hdr field found or EOF 
		 * (ie, no object after hdr) 
		 */

    /* Make sure sender is not getting duplicate notifications */
    if (trans_info.hdr_fields & FROM_F)
      remove_sender (trans_info.sender_addrs, trans_info.notify_addrs,
		     &(trans_info.nnext));
    else if (dump_stdout) { /* We have a tcp user, ie, an IRRj user */
     trans_info.hdr_fields |= FROM_F;
     if(from_ip == NULL)
       from_ip = "localhost";
     sprintf(pbuf,"TCP(%s)", from_ip);
     strcpy (trans_info.sender_addrs, pbuf);
     trans_info.snext += strlen (trans_info.sender_addrs) + 1;
    }
    trans_info.web_origin_str = web_origin_str;
    if (web_origin_str != NULL) {
      sprintf(pbuf,"%s Route Registry Update", trans_info.source ? trans_info.source : "");
      trans_info.subject = strdup (pbuf);
    }
    /* JW this is for debug only */
    /* print_hdr_struct (dfile, &trans_info); */
    chk_email_fields (&trans_info);
    if (!(trans_info.hdr_fields & OP_F))
      trans_info.op = strdup ("UPDATE");

    if (!(trans_info.hdr_fields & OBJ_KEY_F))
      trans_info.obj_key = strdup ("");

    if (!(trans_info.hdr_fields & OBJ_TYPE_F))
      trans_info.obj_type = strdup ("");

    if (chk_hdr_flds (trans_info.hdr_fields)) {
      build_notify_responses (tr, tmpfname, fin, &trans_info, p, db_admin, 
			      MAX_IRRD_OBJ_SIZE, null_notification);
      if ((trans_info.hdr_fields & OLD_OBJ_FILE_F) &&
	  (*trans_info.old_obj_fname < '0' || *trans_info.old_obj_fname > '9'))
	unlink (trans_info.old_obj_fname);
    }
    /*else
      fprintf (dfile, "nofify () bad hdr file found, skipping...\n");*/

    free_ti_mem (&trans_info);
    p = p->next;
  }

  send_notifies (tr, null_notification, ack_fd, dump_stdout);
  remove_tempfiles (tr);
  /*fprintf (dfile, "Exit notify ()\n");*/
}


/* Close and remove all the notification temp files.
 *
 * Return:
 *  void
 */
void remove_tempfiles (trace_t *tr) {
  int i;

  for (i = 0; i < num_hdls; i++) {
    fclose (msg_hdl[i].fp);
    remove (msg_hdl[i].fname);
  }
}

/* Move to this function in phase transition 2 when we convert
 * irr_submit/irrd to transaction 
 *
 * 'submission_count_lines' is a count of the non-null lines
 * in the submission.  Equal to 0 means the user supplied an
 * empty submission.
 * 
 * Return:
 *
 */
void perform_transactions_new (trace_t *tr, FILE *fin, ret_info_t *start,
			       int submission_count_lines, 
			       char *IRRd_HOST, int IRRd_PORT, char *pgpdir) {
  int all_noop;
  char *ret_code;
  irrd_result_t *p;
  
  /* rewind */
  fseek (fin, 0L, SEEK_SET);

/* JW:!!!!: Need to have pick_off_header_info () check for all NOOP's
 * want to avoid sending a transaction in which all objects are NOOP's
 */

  /* Loop through the submission file, initializing the 'ti'
   * struct with the header info and added to the linked list of 'ti's
   * pointed to by 'start'.  If any errors (user or server)
   * are encountered then abort the transaction.
   */
  if (pick_off_header_info (tr, fin, submission_count_lines, start, &all_noop)) {
    trace (NORM, tr, "Submission error(s) found.  Abort transaction.\n");

    /* We are aborting the transaction.  So set the proper
     * user return information to let the user know that the objects 
     * with no errors are being skipped because of transaction
     * semantics.
     */
    reinit_return_list (tr, start, SKIP_RESULT);
  }
  else if (submission_count_lines == 0)
    trace (ERROR, tr, "NULL submission.\n");
  /* if all the obj's in the trans are NOOP's then don't send to irrd/rps-dist */
  else if (!all_noop) {

    trace (NORM, tr, "calling irrd_trans ().\n");

    /* Send the transaction to rps dist to be added to the DB */
    ret_code = irrd_transaction_new (tr, (char *) WARN_TAG, fin, start,
				     IRRd_HOST, IRRd_PORT);

    trace (NORM, tr, "return from irrd_trans ().\n");

    /* Remove any key certificate files from our temp directory area */
    for (p = start->first; p != NULL; p = p->next) {
      /* check for no IRRd errors and we have a key-cert object */
      if (!put_transaction_code_new (tr, p, ret_code) &&
	  is_keycert_obj (p))
	update_pgp_ring_new (tr, p, pgpdir);
      if (p->keycertfn != NULL)
	remove (p->keycertfn);
    }
  }
  
  trace (NORM, tr, "exit perform_transactions_new ().\n");
}


/*
 *
 * 'submission_count_lines' is a count of the non-null lines
 * in the submission.  Equal to 0 means the user supplied an
 * empty submission.
 * 
 * Return:
 *
 */
void perform_rpsdist_trans (trace_t *tr, FILE *fin, ret_info_t *start,
			    int submission_count_lines, char *tlogfn,
			    long tlog_fpos, char *IRRd_HOST, int IRRd_PORT, 
			    char *dbdir) {
  int all_noop;
  char *ret_code;
  irrd_result_t *p;
  
  /* rewind */
  fseek (fin, 0L, SEEK_SET);

/* JW:!!!!: need a check to make sure all the sources are consistent,
 * ie, cannot accept a transaction that spans multiple source DB's
 */

  /* Loop through the submission file, initializing the 'ti'
   * struct with the header info and added to the linked list of 'ti's
   * pointed to by 'start'.  If any errors (user or server)
   * are encountered then abort the transaction.
   */
  if (pick_off_header_info (tr, fin, submission_count_lines, start, &all_noop)) {
    trace (NORM, tr, "Submission error(s) found.  Abort transaction.\n");

    /* We are aborting the transaction.  So set the proper
     * user return information to let the user know that the objects 
     * with no errors are being skipped because of transaction
     * semantics.
     */
    reinit_return_list (tr, start, SKIP_RESULT);
  }
  else if (submission_count_lines == 0)
    trace (NORM, tr, "Empty submission.\n");
  /* if all the obj's in the trans are NOOP's then don't send to irrd/rps-dist */ 
  else if (!all_noop) {
    /* send the transaction to RPS-DIST */
    ret_code = send_rps_dist_trans (tr, fin, start, dbdir, tlogfn, tlog_fpos,
				    IRRd_HOST, IRRd_PORT);

    if (ret_code == NULL)
      trace (NORM, tr, "perform_rpsdist_trans (): back from send_rps_dist_trans (), good trans!\n");
    else
      trace (NORM, tr, "perform_rpsdist_trans (): back from send_rps_dist_trans () bad trans (%s)\n", ret_code);

    /* Place the irrd result in our linked list for notifications */
    for (p = start->first; p != NULL; p = p->next) {
      if (!(p->svr_res & NOOP_RESULT))
	put_transaction_code_new (tr, p, ret_code);
      /* remove any inital PGP public key files */
      if (p->keycertfn != NULL)
	remove (p->keycertfn);
    }
  }
  
  trace (NORM, tr, "exit perform_rpsdist_trans ()\n");

  return;
}

/* Build the communication files and send the names to RPS-DIST.  Routine
 * expects that there are no errors in the transaction (auth, syntax, ...)
 * and that there is at least one valid update to be committed by IRRd
 * (ie, not a file of all NOOP's or an empty submission).
 *
 * Input:
 *   -the canonicalized object file (prepended with pipeline headers) (fin)
 *   -pointer to the 'source' DB for this transaction (start)
 *   -pointer to the absolute directory path of the DB cache.  This is where
 *    the RPS-DIST communication files will go (dbdir)
 *   -name of the bcc transaction log (tlogfn)
 *   -starting file position within the transaction log of this transaction.
 *    The user's original entry is needed to build the journal entry for 
 *    RPS-DIST                                                   (tlog_fpos)
 *   -IRRd host.  Might be needed to contact IRRd to determine trans
 *    outcome (IRRd_HOST)
 *   -IRRd port.  Might be needed to contact IRRd to determine trans
 *    outcome (IRRd_PORT)
 *
 * Return:
 *   -NULL to indicate the transaction was successfully committed by IRRd
 *   -A text message indicating a transaction error occured and was rolled
 *    back.  Also is possible transaction outcome can not be determined if
 *    RPS-DIST times out and IRRd cannot be contacted.
 */
char *send_rps_dist_trans (trace_t *tr, FILE *fin, ret_info_t *start,
			   char *dbdir, char *tlogfn, long tlog_fpos,
			   char *IRRd_HOST, int IRRd_PORT) {
  int pgp_updates_count;
  char *ret_code;
  char template_n [1024], irrd_n[1024], pgp_n[1024], journal_n[1024];
  FILE *irrd_f = NULL, *pgp_f = NULL, *journal_f = NULL;

  /* sanity check */
  if (dbdir == NULL) {
    trace (ERROR, tr, "send_rps_dist_trans () Unspecified DB cache.  Abort rps_dist transaction!\n");
    return "SERVER ERROR: Unspecified DB cache.  Please set the 'irr_directory' in your irrd.conf file";
  }

  /* create the irrd, pgp and journal files */
  sprintf (template_n, "%s/irr_submit.XXXXXX", dbdir);
  strcpy (irrd_n,    template_n);
  strcpy (pgp_n,     template_n);
  strcpy (journal_n, template_n);

  if (((irrd_f    = rpsdist_fopen (irrd_n))    == NULL) ||
      ((pgp_f     = rpsdist_fopen (pgp_n))     == NULL) ||
      ((journal_f = rpsdist_fopen (journal_n)) == NULL)) {
    trace (ERROR, tr, "send_rps_dist_trans () rps dist file creation error.  Abort rps_dist transaction!\n");
    ret_code = "SERVER ERROR: rps dist file creation error";
    goto ABORT_TRANS;
  }

  trace (NORM, tr, "Debug.  comm file names:\n");
  trace (NORM, tr, "Debug.  irrd    (%s)\n", irrd_n);
  trace (NORM, tr, "Debug.  pgp     (%s)\n", pgp_n);
  trace (NORM, tr, "Debug.  jentry  (%s)\n", journal_n);


  /* initialize the irrd, pgp and journal files */

  /* This is the !us...!ue data */
  if ((ret_code = init_irrd_updates (tr, fin, irrd_f, start, WARN_TAG)))
    goto ABORT_TRANS;
  fclose (irrd_f);
  irrd_f = NULL;

  /* This is the PGP update data for the server's local pgp ring */
  if ((ret_code = init_pgp_updates (tr, pgp_f, start, template_n,
				    &pgp_updates_count)))
    goto ABORT_TRANS;
  fclose (pgp_f);
  pgp_f = NULL;

  /* 'NULL' file name is a flag value for RPS-DIST that there are
   * no pgp updates */
  if (pgp_updates_count == 0) {
    remove (pgp_n);
    strcpy (pgp_n, "NULL");
  }

  /* This is the journal entry for the user's submission */
  if ((ret_code = init_jentries (tr, tlogfn, tlog_fpos, start->first->source, journal_f)))
    goto ABORT_TRANS;

  fclose (journal_f);

  trace (NORM, tr, "calling rps_dist_trans ()\n");
  /*
  trace (NORM, tr, "Debug.  Bye-bye\n");
  exit (0);
  */

  /* Send the transaction to rps dist to be added to the DB */
  ret_code = rpsdist_transaction (tr, irrd_n, pgp_n, journal_n, 
				  start->first->source, "localhost", 7777);
  
  if (ret_code != NULL)
    trace (NORM, tr, "return from irrd_trans (%s).\n", ret_code);
  else
    trace (NORM, tr, "return from irrd_trans (NULL return) Yuck!\n");

  /* "C\n means IRRd commited the transaction */
  if (!strcmp (ret_code, "C\n"))
    ret_code = NULL;
  /* "cs" means we did not get an IRRd trans result.  So get the "cs"
   * from IRRd to see if the trans succeeded */
  else if ((*ret_code < '0' || *ret_code >'9') &&
	   (get_irrd_cs (tr, start->first->source, IRRd_HOST, IRRd_PORT) < 
	    atol (ret_code)))
    ret_code = "Transaction result not known.  Query IRRd to determine transaction result or resubmit your transaction at a later time.";
  
  return ret_code;

ABORT_TRANS:
    trace (ERROR, tr, "send_rps_dist_trans () Aborting transaction! (%s)\n", ret_code);
    /* clean up our rps dist communication files */
    if (irrd_f != NULL)
      fclose (irrd_f);
    if (pgp_f != NULL)
      fclose (pgp_f);
    if (journal_f != NULL)
      fclose (journal_f);

    if (strcmp (irrd_n, ""))
      remove (irrd_n);
    if (strcmp (pgp_n, "NULL") && 
	strcmp (pgp_n, "")) {
      pgp_files_backout (pgp_n);
      remove (pgp_n);
    }
    if (strcmp (journal_n, ""))
      remove (journal_n);

    return ret_code;
}

/* Get the latest current serial for DB 'source'.
 *
 * Input:
 *   -the DB source to get the current serial for (source)
 *   -the IRRd host (host)
 *   -the IRRd port (port)
 *
 * Return:
 *   -the latest current serial
 *   -0 if there was any type of error
 */
long get_irrd_cs (trace_t *tr, char *source, char *host, int port) {
  char *data;
  regmatch_t cs_rm[2];
  regex_t cs_re;
  char *cs = "^[^:]+:[^:]+:[[:digit:]]+-([[:digit:]]+)\n$";

  /* get the !j output from IRRd */
  data = irrd_curr_serial (tr, source, host, port);

  /* now parse the output and return the currentserial */
  if (data != NULL) {
    regcomp (&cs_re, cs, REG_EXTENDED);
    if (regexec (&cs_re, data, 2, cs_rm, 0) == 0) {
      *(data + cs_rm[1].rm_eo) = '\0';
      return atol ((char *) (data + cs_rm[1].rm_so));
    }
  }

  return 0;
}

/* Open up a streams file.
 *
 * Return:
 *  A stream file pointer if the operation was a success.
 *  NULL if the file could not be opened.
 */
FILE *rpsdist_fopen (char *fname) {
  FILE *fp;
  int  fd;
 
  fd = mkstemp (fname);
  if (fd == -1)
    return NULL;

  if ((fp = fdopen (fd, "w")) == NULL) {
    close(fd);
    return NULL;
  }

  return fp;
}

/* Generate an RFC 2769 timestamp (pg. 13).  Timestamps are of
 * the form:
 * YYYYMMDD HH:MM:SS [+/-]hh:mm
 *
 * eg, 20000717 12:28:51 +04:00
 *
 * Input:
 *   void
 *
 * Return:
 *   an RPS DIST RFC 2769 timestamp
 */
char *rpsdist_timestamp (char *ts) {
  time_t now;

  now = time (NULL);
  strftime (ts, 256, "%Y%m%d %H:%M:%S ", gmtime (&now));

  /* UTC_OFFSET is the [+/-]hh:mm value, ie, the amount
   * we are away from UTC time */
  strcat (ts, UTC_OFFSET);
  return ts;
}

/* Remove any PGP key files from the RPS-DIST cache area.
 * See init_pgp_updates () for the file format.
 *
 * Input:
 *   -name of the PGP control file which has the fully qualified
 *    path name of the PGP key files (pgpfn)
 *
 * Return:
 *   void
 */
void pgp_files_backout (char *pgpfn) {
  int rm = 0;
  char buf[MAXLINE];
  FILE *fin;

  /* sanity check */
  if (pgpfn == NULL ||
      (fin = fopen (pgpfn, "r")) == NULL)
    return; /* we're hosed ... */

  /* remove any PGP key files we may have created in the 
   * RPS-DIST cache area */
  while (fgets (buf, MAXLINE, fin) != NULL) {
    if (rm) {
      buf[strlen (buf) - 1] = '\0'; /* get rid of the '\n' */
      remove (buf);
    }
    rm = !strcmp ("ADD", buf);
  }

  fclose (fin);
}

/* Build the RPS-DIST journal entry.  See RFC 2769, pg 30 for a 
 * description of the journal format.
 *
 * Input
 *   -the name of the trace log where the initial user copy is (logfn)
 *   -the file position with the trace log where the submission begins (fpos)
 *   -the journal entry output file for RPS-DIST (fout)
 *
 * Return:
 *   NULL if no errors occur in building the ouput file (fout)
 *   otherwise a string message explaining the error condition
 */
char *init_jentries (trace_t *tr, char *logfn, long fpos, char *source, FILE *fout) {
  int first_line = 1, in_headers = 1, pgp_reg = 0;
  int passwd = 0, in_sig = 0, need_sig = 1, in_blankline = 0;
  regex_t mailfromre, pgpbegre, pgpsigre, pgpendre, blanklinere, passwdre, EOT;
  char curline[MAXLINE], ts[256], *ret_code;
  FILE *fin;
  char *mailfrom   = "^From[ \t]";
  char *pgpbegin   = "^-----BEGIN PGP SIGNED MESSAGE-----";
  char *pgpsig     = "^-----BEGIN PGP SIGNATURE-----";
  char *pgpend     = "^-----END PGP SIGNATURE-----";
  char *blankline  = "^[ \t]*\n$";
  char *password   = "^password:|^override:";
  char *eot        = "^---\n$";

  /* sanity check */
  if (logfn == NULL) {
    ret_code = "SERVER ERROR: Can't find the initial submission log.";
    goto JENTRY_ABORT;
  }

  /* open the transaction carbon copy log file */
  if ((fin = fopen (logfn, "r")) == NULL) {
    ret_code = "SERVER ERROR: RPS-DIST PGP fopen () error.";
    goto JENTRY_ABORT;
  }

  /* compile our regular expressions */
  regcomp (&mailfromre,  mailfrom,  REG_EXTENDED|REG_NOSUB);
  regcomp (&pgpbegre,    pgpbegin,  REG_EXTENDED|REG_NOSUB);
  regcomp (&pgpsigre,    pgpsig,    REG_EXTENDED|REG_NOSUB);
  regcomp (&pgpendre,    pgpend,    REG_EXTENDED|REG_NOSUB);
  regcomp (&blanklinere, blankline, REG_EXTENDED|REG_NOSUB);
  regcomp (&passwdre,    password,  REG_EXTENDED|REG_ICASE|REG_NOSUB);
  regcomp (&EOT,         eot,       REG_EXTENDED|REG_NOSUB);

  /* seek to the beginning of the submission */
  fseek (fin, fpos, SEEK_SET);

  /* prepend the redistribution header (RFC 2769, section A.2) */
  fprintf (fout, "transaction-label: %s\n", source);
  fprintf (fout, "sequence: 1234567890\n");
  fprintf (fout, "timestamp: %s\n", rpsdist_timestamp (ts));
  fprintf (fout, "integrity: authorized\n\n");

  /* copy the transaction "as-is" from the user for the RPS-DIST
   * journal file */
  while (fgets (curline, MAXLINE, fin) != NULL) {
    if (first_line && regexec (&mailfromre, curline, 0, 0, 0) != 0)
      in_headers = 0;

    first_line = 0;

    /* skip email header to conform to the RPS-DIST redistribution encoding */
    if (in_headers && regexec (&blanklinere, curline, 0, 0, 0) == 0)
      in_headers = 0;
    if (in_headers)
      continue;

    in_blankline = 0;

    /* check for an end of transaction line '---' */
    if (regexec (&EOT, curline, 0, 0, 0) == 0)
      break; /* all done, exit transaction */

    /* check for a password, eg, 'password: foo' and skip to
     * conform to the RPS-DIST redistribution encoding */
    if (!pgp_reg && regexec (&passwdre, curline, 0, 0, 0) == 0) {
      passwd = 1;
      continue; /* skip the regular signature begin */
    }

    /* check for a '-----BEGIN PGP SIGNED MESSAGE-----' and skip it to
     * conform to the RPS-DIST redistribution encoding */
    if (!pgp_reg && regexec (&pgpbegre, curline, 0, 0, 0) == 0) {
      pgp_reg = 1;
      continue; /* skip the regular signature */
    }

    /* check for a '-----BEGIN PGP SIGNATURE-----' and skip if a regular
     * signature to conform to the RPS-DIST redistribution encoding */
    if (regexec (&pgpsigre, curline, 0, 0, 0) == 0) {
      in_sig = 1;
      if (!pgp_reg) {
	fputs ("\nsignature:\n", fout);
	need_sig = 0;
      }
    }
    
    /* skip the PGP sig for regular sig's and keep for detached */
    if (in_sig) {
      /* skip pgp regular sig's */
      if (pgp_reg)
	continue;
      else
	fputs ("+", fout);
    }

    /* check for a '-----END PGP SIGNATURE-----', stop adding
     * '+'s for RPSL line continuation at the beginning of each line */
    if (in_sig && regexec (&pgpendre, curline, 0, 0, 0) == 0)
      in_sig = 0;
      
    /* check for a blank line as the last line.  need to know
     * when adding the 'signature' meta-object as the server
     * add's a blank line after transactions.  on the last
     * transaction in the file there will not be a blank line
     * as the last line (ie, from the server). */
    if (regexec (&blanklinere, curline, 0, 0, 0) == 0)
      in_blankline = 1;
    
    /* write a line to our journal */
    fputs (curline, fout);
  }

  /* add a 'signature' meta-object if one is needed */
  if (need_sig) {
    if (!in_blankline)
      fputs ("\n", fout);

    if (pgp_reg)
      fputs ("signature:\n  <PGP REGULAR>\n", fout);
    else if (passwd)
      fputs ("signature:\n  <CLEARTEXT PASSWORD>\n", fout);
    else
      fputs ("signature:\n  <MAIL FROM>\n", fout);
  }

  /* make purify happy */
  regfree (&mailfromre);
  regfree (&pgpbegre);
  regfree (&pgpsigre);
  regfree (&pgpendre);
  regfree (&blanklinere);
  regfree (&passwdre);
  regfree (&EOT);
  fclose  (fin);

  return NULL;

JENTRY_ABORT: /* if we get here, someth'in went wrong */

  sprintf (curline, "init_jentries() %s  Abort rps_dist transaction!", ret_code);
  trace (ERROR, tr, "%s\n", curline);
  
  return strdup (curline);
}


/* Make the !us...!ue file for submission to IRRd by RPS-DIST.
 *
 * Input:
 *  -pipeline input file with all the canonicalized objects    (fin)
 *   note that each canonicalized object is preceeded with pipeline
 *   header info.
 *  -routine expects a file to be opened and ready for writing (fout)
 *  -a pointer to the begining of the linked list of updates   (start) 
 *
 * Return:
 *   NULL if no errors occur in building the ouput file (fout)
 *   otherwise a string message explaining the error condition
 */
char *init_irrd_updates (trace_t *tr, FILE *fin, FILE *fout, ret_info_t *start,
			 const char *warn_tag) {
  int n, line_cont, line_begin;
  irrd_result_t *p;
  char *ret_code;
  char *OP[] = {"ADD\n\n", "DEL\n\n"}, buf[MAXLINE];

  /* !us<DB source> */
  fprintf (fout, "!us%s\n", start->first->source);

  /* Loop through the submission file and build the 
   * !us...!ue IRRd update file
   */
  for (p = start->first; p != NULL; p = p->next) {
    /* Skip NOOP operations */
    if (!(p->svr_res & NOOP_RESULT)) {

      /* determine the operation, 'ADD' or 'DEL' ... */
      n = 0;
      if (!strcmp (p->op, DEL_OP))
	n = 1;

      /* ... and write the operation to the IRRd update file */
      if (fputs (OP[n], fout) == EOF) {
	ret_code = "SERVER ERROR: RPS-DIST IRRd operation disk error.";
	goto IRRd_ABORT;
      }

      /* seek to the beginning of object */
      fseek (fin, p->offset, SEEK_SET);

      /* line cont in the fgets () sense, not in the rpsl sense.
       * ie, the current input line was larger than our memory buffer 
       */
      line_cont = 0;

      /* copy the canonicalized submission object from the input 
       * file to the IRR update file */
      while (fgets (buf, MAXLINE, fin) != NULL) {
	n = strlen (buf);
	line_begin = !line_cont;
	line_cont = (buf[n - 1] != '\n');

	/* skip possible 'WARNING:...' lines at the end of the object */
	if (line_begin && !strncmp (buf, warn_tag, strlen (warn_tag)))
	  continue;
		
	fputs (buf, fout);

	if (line_begin && buf[0] == '\n') /* looking for a blank line */
	  break;
      }
    }
  }
  /* !ue */
  fprintf (fout, "!ue\n");

  return NULL;

IRRd_ABORT: /* if we get here, someth'in went wrong */

  sprintf (buf, "init_irrd_updates() %s  Abort rps_dist transaction!", ret_code);
  trace (ERROR, tr, "%s\n", buf);
  
  return strdup (buf);
}


/* Build the RPS-DIST PGP file.  RPS-DIST will use the file built
 * in this routine to update the server's local PGP rings.  The format
 * of the file will be like this:
 *
 * DEL
 * 0x....
 *
 * ADD
 * <fully qualified file name> 
 * ...
 *
 * Note: the reason for not writing 'ADD' keys directly to the file
 * is because it is easier to give pgp a file name to add the key
 * then to read it and send it to pgp's stdin.
 *
 * Input:
 *   -routine expects a file to be opened and ready for writing (fout)
 *   -a pointer to the begining of the linked list of updates   (start)
 *
 * Return:
 *   -a count of the number of PGP udpates (pgp_updates_count)
 *   -NULL if there were no errors encountered
 *   -otherwise a string message explaining the error condition
 */
char *init_pgp_updates (trace_t *tr, FILE *fout, ret_info_t *start,
			char *template_n, int *pgp_updates_count) {
  int n;
  irrd_result_t *p;
  char *ret_code;
  char *OP[] = {"ADD\n", "DEL\n"}, buf[MAXLINE], fn[1024];
  FILE *fin = NULL, *dst = NULL;

  /* keep track of the number of PGP udates we find */
  *pgp_updates_count = 0;

  /* Loop through the submission list and collect data on the PGP updates */
  for (p = start->first; p != NULL; p = p->next) {
    if (!(p->svr_res & NOOP_RESULT) &&
	is_keycert_obj (p)) {

      /* determine the operation, 'ADD' or 'DEL' ... */
      n = 0;
      if (!strcmp (p->op, DEL_OP))
	n = 1;

      /* ... and write the operation to the PGP update file */
      if (fputs (OP[n], fout) == EOF) {
	ret_code = "SERVER ERROR: RPS-DIST PGP operation disk error.";
	goto PGP_ABORT;
      }

      /* 'DEL' operation */
      if (n)
	fprintf (fout, "0x%s\n\n", (p->obj_key + 7));
      /* 'ADD' operation */
      else {
	/* make sure the public key exists ... */
	if (p->keycertfn == NULL) {
	  ret_code = "SERVER ERROR: RPS-DIST PGP missing keycert file.";
	  goto PGP_ABORT;
	}

	/* ... now open it */
	if ((fin = fopen (p->keycertfn, "r")) == NULL) {
	  ret_code = "SERVER ERROR: RPS-DIST PGP fopen () error.";
	  goto PGP_ABORT;
	}
	
	/* open the output public key file */
	strcpy (fn, template_n);
	if ((dst = rpsdist_fopen (fn)) == NULL) {
	  ret_code = "SERVER ERROR: RPS-DIST PGP file creation error.";
	  goto PGP_ABORT;
	}

	/* transfer the public key to an rps-dist PGP key update file */	
	while (fgets (buf, MAXLINE, fin) != NULL)
	  fputs (buf, dst);

	/* write the PGP key filename to the RPS-DIST PGP control file */
	fputs (fn, fout);

	/* This is the old code 
	 * open the public key file *
	if ((fin = fopen (p->keycertfn, "r")) == NULL) {
	  ret_code = "SERVER ERROR: RPS-DIST PGP fopen () error.";
	  goto PGP_ABORT;
	}
	* transfer the public key to the rps-dist PGP update file *
	while (fgets (buf, MAXLINE, fin) != NULL)
	  fputs (buf, fout);
	End of old code */

	/* close and remove the input public key file */
	fputs  ("\n", fout);
	fclose (fin);
	/* removal of the initial public key files is done in rpsdist_trans () */
	fclose (dst);
	fin = dst = NULL;
      }
      /* count the number of PGP updates, there could be 0 */
      (*pgp_updates_count)++;
    }
  }

  return NULL;

PGP_ABORT: /* if we get here, someth'in went wrong */

  if (fin != NULL)
    fclose (fin);
  if (dst != NULL)
    fclose (dst);

  sprintf (buf, "init_pgp_updates () %s  Abort rps_dist transaction!", ret_code);
  trace (ERROR, tr, "%s\n", buf);
  
  return strdup (buf);
}


/* This is the old guy.  We are migrating away from this version of performing
 * the transactions.   However this works and is what we will use for now.
 *
 * Our migration path will be like this:
 *
 * (old guy) -> (irr_submit, irrd transaction compliant) -> (rpsdist)
 *
 * To get to the next step, this routine should be phased out and 
 * perform_transactions_new () should be used.
 * Then to get to the last step perform_transactions_new () should be
 * phased out in favor of perform_rpsdist_trans ().
 */
void perform_transactions (trace_t *tr, FILE *fin, ret_info_t *start,
			   int null_submission, 
			   char *IRRd_HOST, int IRRd_PORT, char *pgpdir) {
  char buf[MAXLINE], *ret_code;
  int fd, num_trans = 0;
  int open_conn = 0, abort_trans = 0;
  long offset;
  trans_info_t ti;
  irrd_result_t *p;
/* pgp_data_t pdat; */
  
  /* fprintf (dfile, "\n----\nEnter perform_transactions()\n");*/

  /* rewind */
  fseek (fin, 0L, SEEK_SET);

  while (fgets (buf, MAXLINE, fin) != NULL) {
    if (strncmp (HDR_START, buf, strlen (HDR_START) - 1))
      continue;
    /* JW commented out to dup rawhoisd 
       if (!abort_trans)
       */
    offset = ftell (fin);
    /* fprintf (dfile, "HDR_START offset (%ld)\n", offset);*/
    /* illegal hdr field found or EOF or no object after hdr */
    if (parse_header (tr, fin, &offset, &ti)) {
      abort_trans = 1;
      /* fprintf (dfile, "calling update_trans_outcome_list (internal error)...\n"); */
      update_trans_outcome_list (tr, start, &ti, offset, INTERNAL_ERROR_RESULT,
				 "\" Internal error: malformed header!\"\n");
      free_ti_mem (&ti);
      continue;
    }
    else if (update_has_errors (&ti))
      abort_trans = 1;
    else if (null_submission == 0) {
      update_trans_outcome_list (tr, start, &ti, offset, NULL_SUBMISSION, NULL);
      continue;
    }

    update_trans_outcome_list (tr, start, &ti, offset, 0, NULL);
    free_ti_mem (&ti);
  }

  /* JW commented out to dup rawhoisd 
   * want to bring back for transaction semantic support
  if (abort_trans)
    reinit_return_list (dfile, start, SKIP_RESULT);
  else {
  */
    for (p = start->first; p != NULL; p = p->next) {
      /* JW want to bring back in later, dup rawhoisd
      if (p->svr_res & NOOP_RESULT)
	continue;
	*/
      /* JW take next 3 sections out to reverse rawhoisd behavior */
      if (p->svr_res & INTERNAL_ERROR_RESULT ||
	  p->svr_res & NULL_SUBMISSION) {
	trace (ERROR, tr, 
	       "Internal error or NULL submission.  Object not added to IRRd.\n");
	continue;
      }

      if (p->svr_res & USER_ERROR) {
	trace (NORM, tr, 
	       "Syntax or authorization error.  Object not added to IRRd.\n");
	continue;
      }

      if (p->svr_res & NOOP_RESULT) {
	trace (NORM, tr, "NOOP object.  Object not added to IRRd.\n");
	continue;
      }

      /* what the eff is this segment doing? */
      if (EOF == fseek (fin, p->offset, SEEK_SET))
	  fprintf (stderr, "ERROR: fseek (%ld)\n", p->offset);
      else {
	fgets (buf, MAXLINE, fin);
	/*fprintf (dfile, "irrd_trans () line: %s", buf);*/
	fseek (fin, p->offset, SEEK_SET);
      }
      /* fprintf (dfile, "perform_trans () calling irrd_transaction ()...\n");*/
      ret_code = irrd_transaction (tr, (char *) WARN_TAG, &fd, fin, p->op, 
				   p->source, ++num_trans, &open_conn, 
				   IRRd_HOST, IRRd_PORT);

      /* check for no IRRd errors and we have a key-cert object */
      if (!put_transaction_code (tr, p, ret_code) &&
	  is_keycert_obj (p)                      &&
	  p->op != NULL) {
	update_pgp_ring_new (tr, p, pgpdir);
	/* 
	if (strcmp (p->op, DEL_OP)) {
	  pgp_add (tr, pgpdir, p->keycertfn, &pdat);
	  pgp_free (&pdat);
	}
	else
	  pgp_del (tr, pgpdir, p->obj_key + 7);
	*/
      }
    }

    if (open_conn) {
      end_irrd_session (tr, fd); /* send '!q' */
      fflush (fin);   /* JW only needed when irrd commands are sent to terminal */
      close_connection (fd);
    }

    /* Remove any key certificate files from our temp directory area */
    for (p = start->first; p != NULL; p = p->next)
      if (p->keycertfn != NULL)
	remove (p->keycertfn);

  /* JW want to bring back in later, dup rawhoisd
  }
  */
    /* fprintf (dfile, "Exit perform_transactions()\n----\n");*/
}
