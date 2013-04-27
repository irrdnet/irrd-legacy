/*
 * $Id: call_pipeline.c,v 1.21 2002/10/17 20:16:13 ljb Exp $
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <irrauth.h>

FILE *dfile;
extern trace_t *default_trace;
extern config_info_t ci;

void call_pipeline (trace_t *tr, FILE *fin, char *web_origin_str,
		    int null_notifications, int dump_stdout, int rps_dist_flag) {
  char outfn[256], outfn2[256], logfn[256], tlogfn[256];
  int ret_code, ack_lock = -1, lock_fd = 0;
  long tlog_fpos = -1;
  char pid_string[20];
  FILE *trans_fp = NULL;
  char *from_ip = NULL;
  struct sockaddr_in myaddr;

  /**** Code to get IP address of sender ****/
  int sock_fd = fileno(fin);
  unsigned int sock_length = sizeof(myaddr);

  umask(022);  /* set umask */
  
  if(!getpeername(sock_fd, (struct sockaddr *)&myaddr, &sock_length)) {
    trace(NORM, tr, "irr_submit connection from %s\n",
          inet_ntoa(myaddr.sin_addr));
    from_ip = inet_ntoa(myaddr.sin_addr);
  }
  /**** end IP get code ****/

  sprintf (pid_string, "PID%d\n", (int) getpid ());

  /* open the transaction log  */
  if (ci.log_dir != NULL) {
    sprintf (logfn, "%s/trans.log", ci.log_dir);
    if ((ack_lock = lock_file (&lock_fd, logfn)) != 0)
      trace (ERROR, tr, "Error opening %s: %s\n",
	     logfn, strerror(errno));
    if ((trans_fp = fopen (logfn, "a")) != NULL) {
      fputs (TRANS_SEPERATOR, trans_fp);
      fputs (pid_string, trans_fp);

      /* support for the journal entry for RPS-DIST */
      strcpy (tlogfn, logfn);
      tlog_fpos = ftell (trans_fp);
    }
    else
      trace (ERROR, tr, "Could not open transaction log: %s\n", 
	     logfn);
  }
  else
    trace (ERROR, tr, "No pipeline submission log directory configured!\n");

  /* Decode PGP stuff, and add any auth cookies necessary. PGP needs to
     come first, because it's sensitive to any changes in the file. */

  if (pgpdecodefile_new (fin, outfn, trans_fp, ci.pgp_dir, tr)) {
    trace (ERROR, tr, 
	   "Panic!  Fatal error from pgpdecodefile PID (%s)  ABORT!\n", pid_string);
    if (trans_fp != NULL)
      fclose (trans_fp);

    exit(0);
  }

  fclose (fin);

  if (trans_fp != NULL)
    fclose (trans_fp);

  if (ack_lock == 0)
    unlock_file (lock_fd);
  else if (lock_fd != 0) {
    close (lock_fd);
    lock_fd = 0;
  }

  /* Open the ack log.  We need to lock the next section as the DB is
   * queried and updated.
   */
  if (ci.log_dir != NULL) {
    sprintf (logfn, "%s/ack.log", ci.log_dir);
    if ((ack_lock = lock_file (&lock_fd, logfn)) != 0)
      trace (ERROR, tr, "Error opening %s: %s\n", logfn, strerror(errno));
    if ((trans_fp = fopen (logfn, "a")) != NULL) {
      fputs (TRANS_SEPERATOR, trans_fp);
      fputs (pid_string, trans_fp);
    }
  }


  /* check mail headers, adding cookies while removing the mail headers
   * from the stream; also preserve any PGP cookies already existing.
   * 'ret_code' is 0..n, ie, the number of non-null lines read.  This way
   * we can detect empty submissions.
   */
  if ((ret_code = addmailcookies(tr, dump_stdout, outfn, outfn2)) < 0)	exit(0);
  unlink(outfn);
  
  if (ret_code > 0) {
    /* syntax checking: irr_rpsl_check */
    dfile = stderr;
    dfile = fopen ("/dev/null", "w");

    if (callsyntaxchk (tr, outfn2, outfn, (char *) tmpfntmpl) < 0) {
      trace (ERROR, tr, "** FAILED IRRCHECK **, problem opening up files\n");
      unlink (outfn2);
      exit(0);
    }
    unlink (outfn2);

    /* auth checking: irr_auth */
    dfile = stdout;

    if (auth_check (tr, outfn, outfn2, ci.irrd_host, 
		    ci.irrd_port, ci.super_passwd) < 0) {
      trace (ERROR, tr, "** FAILED AUTHCHECK **, problem opening files\n");
      unlink (outfn);
      exit(0);
    }
    unlink (outfn);
  }

  
  /* notification: irr_notify */
  dfile = stderr;
  callnotify (tr, outfn2, ret_code, null_notifications, dump_stdout, web_origin_str,
	      rps_dist_flag, ci.irrd_host, ci.irrd_port, ci.db_admin, 
	      ci.pgp_dir, ci.db_dir, tlogfn, tlog_fpos, trans_fp, from_ip);
  unlink (outfn2);

  if (trans_fp != NULL)
    fclose (trans_fp);

  /* See if we need to roll the logs */
  if (ack_lock == 0) {
    log_roll ("%s/ack.log", ci.log_dir, 1);
    unlock_file (lock_fd);
  } /* lock file opened but a lock error occured; see lock_file () */
  else if (ack_lock != 1 && lock_fd != 0) 
    close (lock_fd);
  
  log_roll ("%s/trans.log", ci.log_dir, 0);
}
