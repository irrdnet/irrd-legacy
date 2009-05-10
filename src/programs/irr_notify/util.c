/* 
 * $Id: util.c,v 1.12 2001/08/28 17:29:20 gerald Exp $
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <regex.h>
#include <unistd.h>
#include <irr_notify.h>
#include <pgp.h>


/*
 * Could have an email user or TCP user.
 * In case of a TCP user set the email fields
 * to single space for uniform treatment.
 */
void chk_email_fields (trans_info_t *ti) {

  if (ti->subject == NULL)
    ti->subject = strdup (" ");

  if (ti->date == NULL)
    ti->date = strdup (" ");

  if (ti->msg_id == NULL)
    ti->msg_id = strdup (" ");

  ti->hdr_fields |= (SUBJECT_F|DATE_F|MSG_ID_F);
}

/*
 * Return 0 if something wrong is found , ie, a malformed
 * transaction header has been detected.
 * Else return 1.
 */
int chk_hdr_flds (u_int hdr_flds) {

  return ((int) !((~hdr_flds) & NOTIFY_REQUIRED_FLDS));
}

/* Remove the sender from our notify if he/she appears
 * in the list of notifyees.  The sender always gets a response
 * so another notification is redundant.
 *
 * Return:
 *   void
 *   remove the sender's address from *addr_buf
 */
void remove_sender (char *sender, char *addr_buf, char **next) {
  char buf[MAXLINE];
  char *q, *bnext, *p = NULL;
  int found = 0;

  /* Some email ID's appear as 'gerald@merit.edu (Gerald Andrew Winters)'
   * For these cases want to terminate string at first blank so sender
   * does not get an redundant notify
   */
  if ((p = strchr (sender, ' ')) != NULL)
    *p = '\0';

  bnext = buf;
  for (q = addr_buf; q < *next; q += strlen (q) + 1) {
    if (strcasecmp (sender, q)) {
      strcpy (bnext, q);
      bnext += strlen (q) + 1;
    }
    else
      found = 1;
  }

  if (found) {
    memcpy (addr_buf, buf, bnext - buf);
    *next = (addr_buf + (bnext - buf));
  }

  if (p != NULL)
    *p = ' ';
}

/* Interpret the IRRd return code and place the result in the 
 * 'irrd_result_t' struct.  IRRd will return 'C\n' for success
 * or an error otherwise (IRRd should give a text msg indicating
 * what the error was).
 *
 * Return:
 *  0 means the there were no errors from IRRd
 *  1 otherwise
 */
int put_transaction_code_new (trace_t *tr, irrd_result_t *p, char *irrd_ret_code) {
  int ret_code = 0;

  /*fprintf (dfile, "put_transaction_code () irrd result: %s", irrd_ret_code);*/

if (irrd_ret_code == NULL)
  trace (NORM, tr, "put_transaction_code (NULL return code).\n");
else
  trace (NORM, tr, "put_transaction_code (return code(%s)).\n", irrd_ret_code);

  /* JW 7/7/00 Will this work?
  if (irrd_ret_code != NULL && *irrd_ret_code == 'C')
    p->svr_res = SUCCESS_RESULT;
  else {
  */
  if (irrd_ret_code == NULL)
    p->svr_res = SUCCESS_RESULT;
  else {
    p->svr_res = IRRD_ERROR_RESULT;
    /* Skip the 'F' or whatever machine return code
     * but do get the text error msg
     */
    if (irrd_ret_code != NULL)
      p->err_msg = strdup (++irrd_ret_code);
    else
      p->err_msg = strdup ("Error message not available!\n");
    ret_code = 1;
  }

  return ret_code;
}


/* Interpret the IRRd return code and place the result in the 
 * 'irrd_result_t' struct.  IRRd will return 'C\n' for success
 * or an error otherwise (IRRd should give a text msg indicating
 * what the error was).
 *
 * Return:
 *  0 means the there were no errors from IRRd
 *  1 otherwise
 */
int put_transaction_code (trace_t *tr, irrd_result_t *p, char *irrd_ret_code) {
  int ret_code = 0;

  /*fprintf (dfile, "put_transaction_code () irrd result: %s", irrd_ret_code);*/

if (irrd_ret_code == NULL)
  trace (NORM, tr, "put_transaction_code (NULL return code).\n");
else
  trace (NORM, tr, "put_transaction_code (return code(%s)).\n", irrd_ret_code);

  if (irrd_ret_code != NULL && *irrd_ret_code == 'C')
    p->svr_res = SUCCESS_RESULT;
  else {
    p->svr_res = IRRD_ERROR_RESULT;
    /* Skip the 'F' or whatever machine return code
     * but do get the text error msg
     */
    if (irrd_ret_code != NULL)
      p->err_msg = strdup (++irrd_ret_code);
    else
      p->err_msg = strdup ("Error message not available!\n");
    ret_code = 1;
  }

  return ret_code;
}

/* Is the object a 'key-cert' object? 
 *
 * return:
 *  non-zero means we have a key-cert object
 *  0 otherwise
 */
int is_keycert_obj (irrd_result_t *p) {
  if (p->obj_type == NULL)
    return 0;

  return !strcasecmp (p->obj_type, "key-cert");
}

/* JW get rid of */
int put_transaction_code_old (trace_t *tr, FILE *fin, char code, long fpos) {
  long restore_pos;

  if ((restore_pos = ftell (fin)) == -1L)
    return 0;

  if (fseek (fin, fpos, SEEK_SET)) {
    fprintf (stderr, "ERROR: put_transaction_code() fseek(%ld) error!\n", fpos);
    return 0;
  }
  else {
    if (fputc ((int) code, fin) == EOF)
      /*fprintf (dfile, "put_transaction_code() fputc(%c) error\n", code);*/

    if (fseek (fin, restore_pos, SEEK_SET)) {
      /*fprintf (dfile, "ERROR: put_transaction_code() fseek(%ld) restore fpos error!\n", restore_pos);*/
      return 0;
    }
  }

  return 1;
}

void skip_transaction (FILE *fin, long *offset) {
  char buf[MAXLINE];

  while (fgets (buf, MAXLINE, fin) != NULL) {
    *offset += strlen (buf);
    if (buf[0] == '\n')
      break;
  }
}



/* Add the element pointed to by 'obj' to the end of the linked list.
 *
 * Return:
 *  void
 */
void add_outcome_list_obj (trace_t *tr, ret_info_t *start, irrd_result_t *obj) {

  obj->next = NULL;
  if (start->first == NULL)
    start->first = obj;
  else
    start->last->next = obj;

  start->last = obj;
}

/* Initialize an irrd_result_t struct.  If an INTERNAL_ERROR_RESULT, NOOP_RESULT, or
 * USER_ERROR occurs the 'svr_res' field is initialized.  Otherwise 'svr_res'
 * will be uninitialized.  In this case, 'svr_res' will be initialized when
 * the update is applied to IRRd from the IRRd return code.  The 'op', 'source' 
 * and 'offset' fields are used to send the transaction to IRRd.
 *
 * Also see if outcome was a NOOP.  Calling functions need to know if every
 * object was a NOOP.
 *
 * Return:
 *  1 if outcome was a NOOP
 *  0 otherwise
 */
/*
  obj = (irrd_result_t *) calloc (1, sizeof (irrd_result_t));
  */
int update_trans_outcome_list (trace_t *tr, ret_info_t *start, 
				 trans_info_t *ti, long offset, 
				 enum SERVER_RES svr_res, char *err_msg) {
  irrd_result_t *obj;
  int noop_count = 0;

  /* fprintf (dfile, "enter update_trans_outcome_list ()...\n");*/
  obj = (irrd_result_t *) malloc (sizeof (irrd_result_t));
  memset ((char *) obj, 0, sizeof (irrd_result_t));
  if (err_msg != NULL) {
    obj->err_msg = strdup (err_msg);
    obj->svr_res = svr_res;
  }
  else if (svr_res == NULL_SUBMISSION)
    obj->svr_res = NULL_SUBMISSION;
  else if ((ti->hdr_fields & OP_F) && !strcmp (ti->op, NOOP_OP)) {
    obj->svr_res = NOOP_RESULT;
    noop_count = 1;
  }
  else if (ti->hdr_fields & ERROR_FLDS)
    obj->svr_res = USER_ERROR;

  obj->hdr_fields = ti->hdr_fields;
  if (ti->hdr_fields & OP_F)
    obj->op = strdup (ti->op);

  if (ti->hdr_fields & SOURCE_F)
    obj->source = strdup (ti->source);

  if (ti->obj_type != NULL)
    obj->obj_type = strdup (ti->obj_type);

  if (ti->obj_key != NULL)
    obj->obj_key = strdup (ti->obj_key);

  if (ti->keycertfn != NULL)
    obj->keycertfn = strdup (ti->keycertfn);

  obj->offset = offset;
  /* fprintf (dfile, "update_trans_outcome (): offset (%ld)\n", obj->offset);*/

  add_outcome_list_obj (tr, start, obj);
  /* fprintf (dfile, "exit update_trans_outcome_list ()...\n");*/

  return noop_count;
}

/* Set all unset elements in the 'irrd_result_t' list to 'res'.  This
 * function should be called when an error of any kind has been
 * found (eg, syntax error, network error, internal error, ...).
 * The reason is to support transaction semactics which calls for
 * a transaction to be aborted if the entire trans cannot be applied.
 * So this routine will set the proper trans outcome to those updates
 * that would have been sent to IRRd if it were not for the error occuring.
 * 
 * Return:
 *  void
 */
void reinit_return_list (trace_t *tr, ret_info_t *start, enum SERVER_RES res) {
  irrd_result_t *p;

  for (p = start->first; p != NULL; p = p->next)
    if (p->svr_res == 0)
      p->svr_res = res;
}

/* Add or delete the PGP public key to our production ring.
 * 'ir' is a struct with a pointer to the location of 
 * the public key file and 'pgpdir' is a pointer to the
 * location of the production rings.
 *
 * Return:
 *   void
 */
void update_pgp_ring (trace_t *tr, irrd_result_t *ir, char *pgpdir) {
  int p[2], pgp_ok = 0;
  char curline[4096], pgppath[256];
  FILE *pgpout;
  char *pgp_add = "^Keys added successfully";
  char *pgp_del = "^Removed.";
  regex_t re;
  
  printf ("enter update_pgp_ring...\n");
  if (ir->op == NULL ||
      (strcmp (ir->op, ADD_OP) && strcmp (ir->op, DEL_OP)))
    return;

  printf ("operation (%s) key-cert (%s) certfn (%s) pgpdir (%s)\n", 
	  ir->op, ir->obj_key, ir->keycertfn, pgpdir);
  printf ("processing ADD pgp key operation\n");
  
  if (strcmp (ir->op, DEL_OP))
    regcomp (&re, pgp_add, REG_EXTENDED|REG_NOSUB);
  else
    regcomp (&re, pgp_del, REG_EXTENDED|REG_NOSUB); 
  
  /* There is a bug in 5.0i in which command line parms 
   * for the path's to the rings are ignored.  To get
   * around this we must set the PGPPATH environ var.
   */
  strcpy (pgppath, "PGPPATH=");
  strcat (pgppath, pgpdir);
  if (putenv (pgppath)) {
    /* JW This should be written to an error log!!!!! */
    printf ("Coudn't set environment var (%s) for pgp.  Abort!\n", pgpdir);
    return;
  }

  pipe(p);
  if (fork() == 0) { /* Child */
    dup2(p[0], 1);
    dup2(p[0], 2);
    close(p[1]);
    
    if (strcmp (ir->op, DEL_OP))
      execlp (PGPK, PGPK, "--batchmode=1", "-a", ir->keycertfn, NULL); 
    else {
      strcpy (curline, "0x");
      strcat (curline, (ir->obj_key + 7));
      execlp (PGPK, PGPK, "--batchmode=1", "-r", curline, NULL); 
    }
    /* JW this should go into an error log!!!!! */
printf ("child: oops! shouldn't be here, execlp fail\n");    
    _exit(127);
  }
  
  close(p[0]);
  if ((pgpout = fdopen(p[1], "r")) == NULL) {
    /* JW This should be written to an error log!!!!! */
    printf ("Internal error.  Couldn't open pipe: update_pgp_ring (%s)",
	    ir->obj_key);
    return;
  }
  
  /* Look for a successful operation from PGP */
  while (fgets (curline, 4095, pgpout) != NULL) {
    if (0 == regexec (&re, curline, 0, NULL, 0))
      pgp_ok = 1;
  }
  
  fclose (pgpout);
  
  if (!pgp_ok) /* JW This should be written to an error log!!!!! */
    printf ("Oops! Couldn't (%s) pgp key (%s) to local ring\n", 
	    ir->op, ir->obj_key);
}




/* Loop through the submission file 'fin', initializing the 'ti'
 * struct with the header info and adding to a global linked list
 * pointed to by 'start'.  Most important is to see if there are
 * any errors in the submission (eg, syntax error, auth error, ...)
 * which should cause the entire transaction to abort.
 *
 * Also determine if all objects in the submission are NOOP's.
 *
 * Input:
 *    -pipeline input file with info headers (fin)
 *    -flag to indicate there were no objects in the trans (submission_count_lines)
 *    -pointer to the linked list of trans info structures (start)
 *
 * Return:
 *    -1 If any errors (user or server) are encountered, this signals
 *       the calling routine to abort the transaction 
 *    -0 otherwise
 *    -flag value (0 or 1) to indicate if all objects in trans are NOOP's (all_noop) 
 */
int pick_off_header_info (trace_t *tr, FILE *fin, int submission_count_lines,
			  ret_info_t *start, int *all_noop) {
  int corrupt_hdr, abort_trans = 0, obj_count = 0, noop_count = 0;
  long offset;
  trans_info_t ti;
  char buf[MAXLINE];

  /* initialize.  if all objects are NOOP's then return all_noop = 1 to 
   * calling function */
  *all_noop = 0;

  while (fgets (buf, MAXLINE, fin) != NULL) {
    if (strncmp (HDR_START, buf, strlen (HDR_START) - 1))
      continue;

    corrupt_hdr = 0;

    /* Parse the header lines HDR-START ... HDR-END, fill in 
     * the 'ti' struct with the header info and return 0 if 
     * a 'HDR-END' field is encountered */
    if (parse_header (tr, fin, &offset, &ti)) {
      corrupt_hdr = abort_trans = 1;
    }
    else if (update_has_errors (&ti))
      abort_trans = 1;

    if (corrupt_hdr)
      update_trans_outcome_list (tr, start, &ti, offset, INTERNAL_ERROR_RESULT,
				 "\" Internal error: malformed header!\"\n");
    else if (submission_count_lines == 0)
      update_trans_outcome_list (tr, start, &ti, offset, NULL_SUBMISSION, NULL);
    else
      noop_count += update_trans_outcome_list (tr, start, &ti, offset, 0, NULL);

    free_ti_mem (&ti);
    obj_count++;
  }

  /* Return if all the objects in the transaction were NOOP's */
  *all_noop = (obj_count == noop_count);

  return abort_trans;
}


/* Add or delete the PGP public key to/from our production ring.
 *
 * Input:
 *   -pointer to a struct with the file name of the key file, the 
 *    operation (ie, add or del) and the hex id of the key (ir)
 *   -a fully qualified path name to the pgp rings (pgpdir)
 *
 * Return:
 *   void
 */
void update_pgp_ring_new (trace_t *tr, irrd_result_t *ir, char *pgpdir) {
  
fprintf (stderr, "JW:update_pgp_ring_new() pgpdir (%s)\n", pgpdir);
  /* sanity checks */
  if (ir == NULL || ir->op == NULL || ir->obj_key == NULL) {
    trace (ERROR, tr, "update_pgp_ring () NULL result pointer...\n");  
    return;
  }
  else if (ir->op == NULL) {
    trace (ERROR, tr, "update_pgp_ring () NULL op for key/hex ID (%s)\n",
	   (ir->obj_key != NULL ? ir->obj_key : "NULL"));  
    return;
  }
  else if (ir->obj_key == NULL) {
    trace (ERROR, tr, "update_pgp_ring () NULL key/hex ID\n");  
    return;
  }

  /* if it's a 'REPLACE_OP' or 'NOOP', nothing to do to rings */
  if (strcmp (ir->op, ADD_OP) && strcmp (ir->op, DEL_OP))
    return;
  
  /* check the hexid which is in 'PGPKEY-........' format */
  if (!pgp_hexid_check ((ir->obj_key + 7), NULL)) {
    trace (ERROR, tr, " update_pgp_ring () malformed hex_id\n");
    return;
  }
  else if (pgpdir == NULL) {
    trace (ERROR, tr, 
	   "update_pgp_ring () Yikes! NULL PGP dir/PGPPATH pointer\n");  
    return;    
  }

  /* everything looks good, now update our rings */

  /* add a key to our local ring */
  if (strcmp (ir->op, DEL_OP)) {
    if (!pgp_add (tr, pgpdir, ir->keycertfn, NULL))
      trace (ERROR, tr, "update_pgp_ring () pgp error in adding key (%s)\n",
	     ir->obj_key);  
  }
  /* remove a key from our local ring */
  else if (!pgp_del (tr, pgpdir, (ir->obj_key + 7))) {
    trace (ERROR, tr, "update_pgp_ring () pgp error in deleting key (%s)\n",
	   ir->obj_key);
  }

fprintf (stderr, "JW: exit update_pgp_ring_new () ...\n");
}
