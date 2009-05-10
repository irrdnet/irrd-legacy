/* 
 * $Id: notify_msgs.c,v 1.6 2002/10/17 20:25:56 ljb Exp $
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <irr_notify.h>

extern config_info_t ci;

/*
 * Move the file position pointer beyond the current object.
 * It is possible through NOOP's and/or no notifers and/or
 * forwarder's that the current object has not been used
 * for anything.  So we need to move on to the next transaction
 * in the file.
 */
void read_past_obj (FILE *fp) {
  char buf[MAXLINE];

  while (fgets (buf, MAXLINE, fp) != NULL) {
    if (buf[0] == '\n')
      break;
  }
}

/*
 * Dump the object to file.  Every object should have
 * a blank line after it, even the last object in the file.
 */
/* JW need to put a check for "large objects".  When object exeeds
 * size limit, [snip...] is printed and rest of object is skipped.
 */
int dump_object_to_file (trace_t *tr, FILE *fp, FILE *fin, long obj_pos,
			 int max_line_size) {
  char buf[MAXLINE];
  char *cp;
  int count = 0, print_snip = 1;
  
  strcpy (buf, "\n");
  trace (TR_TRACE, tr, "dump_object_to_file () dumping object to file...\n");
  fseek (fin, obj_pos, SEEK_SET);
  while ((cp = fgets (buf, MAXLINE, fin)) != NULL) {
    count++;
    if (buf[0] == '\n')
      break;

    if (--max_line_size < 0 && 	  
	!(is_changed (buf)       ||
	  is_source (buf)        ||
	  buf[0] == ERROR_TAG[0] ||
	  buf[0] == WARN_TAG[0])) {
      if (print_snip && max_line_size < 0) {
	fputs (SNIP_MSG, fp);
	print_snip = 0;
      }
      continue;
    } 
    fputs (buf, fp);
  }
  
  if (cp == NULL && 
      count < 4)
    return -1;

  /* want to make sure next line written is not concated to this one */
  if (buf[strlen (buf) - 1] != '\n')
    fprintf (fp, "\n");

  return 1;
}

/* Copy the file whose name is '*old_fname' to the file pointed
 * bye '*msg_fd' (ie, the user notification file).  If the file
 * name begins with a digit, function assumes the name is an offset
 * in the input file.  Otherwise the file is opened and copied to
 * the notification file with no special treatment.
 *
 * Return:
 *   1 if there were no errors
 *  -1 otherwise
 */
int dump_old_obj (trace_t *tr, FILE *msg_fp, FILE *fin, char *old_fname, 
		  int max_line_size) {
  FILE *fp_old;
  long fpos;
  int ret_code;
  
  /*fprintf (dfile, "\n---\ndump_old_obj() file name-(%s)\n", old_fname);*/
  
  /* file pointer is an offset in the input file */
  if (*old_fname >= '0' && *old_fname <= '9') {
    fpos = ftell (fin);
    ret_code = dump_object_to_file (tr, msg_fp, fin, strtol (old_fname, NULL, 10),
				    max_line_size);
    fseek (fin, fpos, SEEK_SET);
  }
  else if ((fp_old = fopen (old_fname, "r")) == NULL) {
    trace (NORM, tr, "ERROR: dump_old_obj () could not open file \"%s\"\n", 
	   old_fname);
    ret_code = -1;
  }
  else {
    ret_code = dump_object_to_file (tr, msg_fp, fp_old, 0L, max_line_size);
    fclose (fp_old);
  }

  /* fprintf (dfile, "dump_old_obj() file name-(%s) ret_code-(%d)\n---\n\n", old_fname, ret_code);*/
  return ret_code;
}

void maint_request (trace_t *tr, FILE *fin, long obj_pos, trans_info_t *ti,
                        char *to_addr, char *tmpfname, int max_line_size ) {
  long fpos;
  FILE *fp;
  char buf[MAXLINE], email[256], *from_addr;
  int fd, status, pid, w, no_mail = 0;

  fpos = ftell (fin);

  /* create the file name */
  strcpy(buf, tmpfname);
  fd = mkstemp(buf);

  if ((fp = fdopen (fd, "w")) == NULL) {
    trace (NORM, tr, "maint_request () could not open file \"%s\"\n", buf);
    return;
  }

#ifdef HAVE_SENDMAIL
  if (ti->sender_addrs != NULL)
    from_addr = ti->sender_addrs;
  else
    from_addr = "nobody@localhost"; 
  fprintf (fp, "From: %s\nReply-To: %s\n", from_addr, from_addr);
  fprintf (fp, "To: %s\nSubject: %s\n", to_addr, ti->subject);
#endif

  if (ti->new_mnt_error)
    fputs ("\n-- New maintainer request --\n\n", fp);
  else
    fputs ("\n-- Maintainer deletion request (passed auth check) --\n\n", fp);
  if (ti->sender_addrs != NULL)
    fprintf (fp, "-FROM: %s\n", ti->sender_addrs);
  if (ti->date != NULL)
    fprintf (fp, "-DATE: %s\n", ti->date);
  if (ti->web_origin_str != NULL)
    fprintf (fp, WEB_UPDATE, ti->web_origin_str);

  if (dump_object_to_file (tr, fp, fin, obj_pos, max_line_size) > 0) {
    trace (TR_TRACE, tr, "maint_request () sending mail to (%s)...\n", email
);
    fclose (fp);

#ifdef HAVE_SENDMAIL
    sprintf (email, "%s < %s", SENDMAIL_CMD, buf);
#elif HAVE_MAIL
    sprintf (email, "%s \"%s\" < %s", MAIL_CMD, to_addr, buf);
#else
    trace (NORM, tr, "No mail or sendmail found\n");
    trace (NORM, tr, "Could not send a \"new maintainer\" request msg\n");
    no_mail = 1;
#endif

    if (!no_mail) {
      trace (NORM, tr, "mail %s\n", to_addr);
      chmod (email, 00666);
      if ((pid = fork ()) == 0) {
	execlp ("sh", "sh", "-c", email, NULL);
	exit (127);
      }
      while ((w = wait (&status)) != pid && w != -1);
    }
  }
  else
    fclose (fp);

  fseek (fin, fpos, SEEK_SET);
  unlink (buf);
}

/* 
 * Create a notification/forward file.  Save the file pointer
 * in a global array of file pointers (ie, msg_hdl[]).  num_hdls
 * gives the number of file pointers in msg_hdl[] and also
 * points to the next available index.
 */
int create_notify_file (trace_t *tr, char *p, char *tmpfname) {
  char buf[MAXLINE];
  int fd;

  /* create the file name */
  strcpy(buf, tmpfname);
  fd = mkstemp (buf);

  if (num_hdls >= MAX_HDLS || fd == -1) {
    trace (NORM, tr, "ERROR: create_notify_file () file hdl overflow (%d) or tempfile error\n", 
	   num_hdls);
    close(fd);
    return -1;
  }
  else if ((msg_hdl[num_hdls].fp = fdopen (fd, "w+")) == NULL) {
    trace (NORM, tr, 
	   "ERROR: create_notify_file() could not open file \"%s\"\n", buf);
    close(fd);
    unlink(buf);
    return -1;
  }
  
  msg_hdl[num_hdls].fname = strdup (buf);
  return num_hdls++;
}

/* return index of element in list or return -1 */
int present_in_list (char *list, char *last, char *target_str) {
  char *p;
  int i = 0;

  for (p = list; p < last; p += strlen (p) + 1, i++)
    if (!strcasecmp (target_str, p))
      return i;

  return -1;
}

/* Add mail addr *p to list (ie, notify or forward list) 
 * pointed to by *next.
 */
int add_list_member (trace_t *tr, char *p, char *buf, char **next) {

  if ((MAX_ADDR_SIZE - (*next - buf)) > strlen (p)) {
    strcpy (*next, p);
    *next += strlen (p) + 1;
    return 1;
  }
  else {
    trace (NORM, tr, "ERROR: add_list_member () buffer ?_addrs overflow!\n");
    return -1;
  }

}

void init_response_header (trace_t *tr, FILE *fp, char *from, char *to, enum NOTIFY_T response_type,
			   trans_info_t *ti, char *db_admin) {

#ifdef HAVE_SENDMAIL
  if (ti->web_origin_str == NULL || response_type != SENDER_RESPONSE) {
    if (db_admin != NULL)
      fprintf (fp, "From: %s\nReply-To: %s\n", db_admin, db_admin);
    fprintf (fp, "To: %s\nSubject: %s\n", to, ti->subject);
  }
#endif

  switch (response_type) {
  case SENDER_RESPONSE:
    fprintf (fp, SENDER_HEADER);
    break;
  case FORWARD_RESPONSE:
    if (ci.forward_header_msg != NULL)
      fprintf (fp, ci.forward_header_msg);
    else
      fprintf (fp, FORWARD_HEADER);
    break; 
  case NOTIFY_RESPONSE:
    if (ci.notify_header_msg != NULL)
      fprintf(fp, ci.notify_header_msg);
    else
      fprintf (fp, NOTIFY_HEADER, ti->source);
    break;
  default:
    trace (NORM, tr, 
	   "ERROR: init_response_header() unknown repsone type-(%d)\n", 
	   response_type);
    break;
  }

  fprintf (fp, DIAG_HEADER);

  if (ti->web_origin_str == NULL)
    fprintf (fp, MAIL_HEADERS, from, ti->subject, ti->date, ti->msg_id);
  else if ( response_type != SENDER_RESPONSE )
    fprintf (fp, WEB_UPDATE, ti->web_origin_str);

}

void init_response_footer (FILE *fp) {

  if (ci.footer_msg != NULL)
    fprintf (fp, ci.footer_msg);
  else {
    fprintf (fp, RESPONSE_FOOTER);
  }
}

/*
 * Create a response file and init the file with the response header 
 * text.
 */
void init_response (trace_t *tr, char *tmpfname, char *from, char *addrs_buf, 
		    char *last, char *gbl_buf, char **gbl_last, int ndx[], 
		    int *next_ndx, enum NOTIFY_T response_type, trans_info_t *ti,
		    char *db_admin) {
  char *p;
  int j;

  /* fprintf (dfile, "Enter init_responses()\n");*/
  
  for (p = addrs_buf; p  < last; p += strlen (p) + 1) {
    /* fprintf (dfile, "init_response () examine addr-(%s)\n", p);*/
    if ((j = present_in_list (gbl_buf, *gbl_last, p)) < 0) {
      /* fprintf (dfile, "\"%s\" not in list, adding...\n", p);*/
      if (add_list_member (tr, p, gbl_buf, gbl_last) < 0) {
	return;
      }
      if ((j = create_notify_file (tr, p, tmpfname)) < 0) {
	return;
      }
      init_response_header (tr, msg_hdl[j].fp, from, p, response_type, ti, db_admin);
      ndx[*next_ndx] = j;
      (*next_ndx)++;
      /*fprintf (dfile, "init_response(): create_notify_file: (file hdl num-(%d))\n", ndx[j]);*/
    }
  }
}

/*
 * JW later must look at length of maint list and break up
 * onto additional lines if necessary.
 */
void dump_maint_list (FILE *fp, char *maint_list) {
  fprintf (fp, "%s%s", ERROR_TAG, maint_list);
}

void forwarder_response (trace_t *tr, FILE *fin, long obj_pos, trans_info_t *ti, 
			 FILE *msg_fp) {

  fprintf (msg_fp, "%s", MSG_SEPERATOR);

  if (!strcmp (ti->op, REPLACE_OP))
    fprintf (msg_fp, "%s", FORWARD_REPL_MSG);
  else if (!strcmp (ti->op, DEL_OP))
    fprintf (msg_fp, "%s", FORWARD_DEL_MSG);
  else
    fprintf (msg_fp, "%s", FORWARD_ADD_MSG);

  dump_object_to_file (tr, msg_fp, fin, obj_pos, 10000);  
}

void notifier_response (trace_t *tr, FILE *fin, long obj_pos, trans_info_t *ti, 
			FILE *msg_fp, int max_obj_line_size) {

  /*fprintf (dfile, "notifier_response(op-(%s))\n", ti->op);*/
  fprintf (msg_fp, "%s", MSG_SEPERATOR);

  if (!strcmp (ti->op, REPLACE_OP)) {
    fprintf (msg_fp, "%s", PREV_OBJ_MSG);
    dump_old_obj (tr, msg_fp, fin, ti->old_obj_fname, max_obj_line_size);
    fprintf (msg_fp, "%s", NOTIFY_REPL_MSG);
  }
  else if (!strcmp (ti->op, DEL_OP)) {
    fprintf (msg_fp, "%s", NOTIFY_DEL_MSG);
    dump_old_obj (tr, msg_fp, fin, ti->old_obj_fname, max_obj_line_size);
  }
  else
    fprintf (msg_fp, "%s", NOTIFY_ADD_MSG);

  if (strcmp (ti->op, DEL_OP))
   dump_object_to_file (tr, msg_fp, fin, obj_pos, max_obj_line_size); 
}


void sender_response (trace_t *tr, FILE *fin, long obj_pos, trans_info_t *ti, 
		      FILE *msg_fp, irrd_result_t *irrd_res, 
		      char *db_admin, char *tmpfname, int max_obj_line_size) {
  char buf[MAXLINE];

  /* print the transaction outcome */
  if (irrd_res->svr_res & SUCCESS_RESULT) {
    fprintf (msg_fp, SENDER_OP_SUCCESS, ti->op, ti->obj_type, ti->obj_key);
    trace (NORM, tr, SENDER_OP_SUCCESS, ti->op, ti->obj_type, ti->obj_key);
    if (ti->syntax_warns)
      dump_object_to_file (tr, msg_fp, fin, obj_pos, max_obj_line_size);

    return;
  }

  if (irrd_res->svr_res & NOOP_RESULT) {
    fprintf (msg_fp, SENDER_OP_NOOP, ti->obj_type, ti->obj_key);
    trace (NORM, tr, SENDER_OP_NOOP, ti->obj_type, ti->obj_key);
    return;
  }

  if (irrd_res->svr_res & NULL_SUBMISSION) {
    fprintf (msg_fp, NULL_SUBMISSION_MSG);
    trace (NORM, tr, NULL_SUBMISSION_MSG, ti->op, ti->obj_type, ti->obj_key);
    return;
  }

  if (irrd_res->svr_res & IRRD_ERROR_RESULT) {
    fprintf (msg_fp, SENDER_NET_ERROR, ti->op, ti->obj_type, ti->obj_key, 
	     irrd_res->err_msg);
    trace (NORM, tr, SENDER_NET_ERROR, ti->op, ti->obj_type, ti->obj_key, 
	   irrd_res->err_msg);
    return;
  }
   
  if (irrd_res->svr_res & INTERNAL_ERROR_RESULT) {
    fprintf (msg_fp, INTERNAL_ERROR, ti->op, ti->obj_type, ti->obj_key, 
	     irrd_res->err_msg);
    trace (NORM, tr, INTERNAL_ERROR, ti->op, ti->obj_type, ti->obj_key, 
	     irrd_res->err_msg);
    return;
  }    

  if (irrd_res->svr_res & SKIP_RESULT) {
    fprintf (msg_fp, SENDER_SKIP_RESULT, ti->op, ti->obj_type, ti->obj_key);
    trace (NORM, tr, SENDER_SKIP_RESULT, ti->op, ti->obj_type, ti->obj_key);
    return;
  }    

  fprintf (msg_fp, "\n");
  fprintf (msg_fp, SENDER_OP_FAILED, ti->op, ti->obj_type, ti->obj_key);
  dump_object_to_file (tr, msg_fp, fin, obj_pos, max_obj_line_size);
  trace (NORM, tr, SENDER_OP_FAILED, ti->op, ti->obj_type, ti->obj_key);

  if (ti->syntax_errors) {
    fprintf (msg_fp, "\n");
    trace (NORM, tr, "%s%s\n", ERROR_TAG, "Syntax errors");
    return;
  }

  if (ti->del_no_exist) {
    sprintf (buf, "%s%s", ERROR_TAG, DEL_NO_EXIST_MSG);
    fprintf (msg_fp, buf, ti->source);
    fprintf (msg_fp, "\n");
    trace (NORM, tr, buf, ti->source);
    return;
  }

  if (ti->maint_no_exist != NULL) {
    fprintf (msg_fp, "%s%s", ERROR_TAG, MAINT_NO_EXIST_MSG);
    dump_maint_list (msg_fp, ti->maint_no_exist);
    fprintf (msg_fp, "\n");
    trace (NORM, tr, "%s%s", ERROR_TAG, MAINT_NO_EXIST_MSG);
    return;
  }

  if (ti->bad_override) {
    sprintf (buf, "%s%s", ERROR_TAG, BAD_OVERRIDE_MSG);
    if (ti->override != NULL)
      fprintf (msg_fp, buf, ti->override);
    else
      fprintf (msg_fp, buf, "?");
    trace (NORM, tr,  BAD_OVERRIDE_MSG);
    fprintf (msg_fp, "\n");
    return;
  }

  if (ti->unknown_user) {
    fprintf (msg_fp, "%s%s", ERROR_TAG, UNKNOWN_USER_MSG);
    return;
  }

  if (ti->new_mnt_error) {
    if (db_admin != NULL) {
      fprintf (msg_fp, "%s%s %s", ERROR_TAG, NEW_MNT_ERROR_MSG_2, db_admin);
      maint_request (tr, fin, obj_pos, ti, db_admin, tmpfname,
			max_obj_line_size);
    }
    else
      fprintf (msg_fp, "%s%s", ERROR_TAG, NEW_MNT_ERROR_MSG);

    fprintf (msg_fp, "\n");
    trace (NORM, tr, "%s%s", ERROR_TAG, NEW_MNT_ERROR_MSG);
    return;
  }

  if (ti->del_mnt_error) {
    if (db_admin != NULL) {
      fprintf (msg_fp, "%s%s %s", ERROR_TAG, DEL_MNT_ERROR_MSG_2, db_admin);
      maint_request (tr, fin, obj_pos, ti, db_admin, tmpfname, 
			 max_obj_line_size);
    }
    else
      fprintf (msg_fp, "%s%s", ERROR_TAG, DEL_MNT_ERROR_MSG);

    fprintf (msg_fp, "\n");
    trace (NORM, tr, "%s%s", ERROR_TAG, DEL_MNT_ERROR_MSG);
    return;
  }

  if (ti->authfail) {
    fprintf (msg_fp, "%s%s", ERROR_TAG, AUTHFAIL_MSG);
    fprintf (msg_fp, "\n");
    if (!strcmp (ti->op, REPLACE_OP)) {
      char *notify_str;

      fprintf (msg_fp, EXIST_OBJ_MSG);
      dump_old_obj (tr, msg_fp, fin, ti->old_obj_fname, max_obj_line_size);
      if (db_admin == NULL)
	notify_str = "the existing maintainer";
      else
	notify_str = db_admin;
      fprintf (msg_fp, REPLACE_FAIL_NOTE, db_admin);
    }
    trace (NORM, tr, "%s%s", ERROR_TAG, AUTHFAIL_MSG);
    return;
  }

  if  (ti->otherfail) {
    fprintf (msg_fp, "%s%s\n", ERROR_TAG, ti->otherfail);
    fprintf (msg_fp, "\n");
    trace (NORM, tr, "%s%s\n", ERROR_TAG, ti->otherfail);
    return;
  }
}

void build_notify_responses (trace_t *tr, char *tmpfname, FILE *fin, 
 			     trans_info_t *ti, irrd_result_t *irrd_res,
			     char *db_admin, int max_obj_line_size, 
			     int null_notification) {
  char *p;
  int j;
  long fpos = ftell (fin);

  /* Response to sender */  
  if (snext == sender_addrs){
    init_response (tr, tmpfname, ti->sender_addrs, ti->sender_addrs, ti->snext,
                   sender_addrs, &snext, sndx, &num_sender, SENDER_RESPONSE, ti,
                   db_admin);
  }
  sender_response (tr, fin, fpos, ti, msg_hdl[sndx[0]].fp, irrd_res, 
		   db_admin, tmpfname, max_obj_line_size);


  if (null_notification)
    return;

  /* Response to forwarder's (ie, 'upd-to:'s) */
  if (irrd_res->hdr_fields & AUTHFAIL_F) {
    init_response (tr, tmpfname, ti->sender_addrs, ti->forward_addrs, ti->fnext, 
		   forward_addrs, &fnext, fndx, &num_forward, FORWARD_RESPONSE, ti,
                   db_admin);
    
    for (p = ti->forward_addrs; p  < ti->fnext; p += strlen (p) + 1) { 
      if ((j = present_in_list (forward_addrs, fnext, p)) < 0) {
	trace (ERROR, tr, "build_notify_responses() \"%s\" not in forward list\n", 
	       p);
	continue;
      }
      trace (NORM, tr, "upd-to: %s\n", p);
      forwarder_response (tr, fin, fpos, ti, msg_hdl[fndx[j]].fp);
    }
    return;
  }

  /* want to allow for the possiblility of auth fail 
   * and no maint exist conditions
   */
  if (ti->maint_no_exist != NULL)
    return;

  if (!(irrd_res->svr_res & SUCCESS_RESULT))
    return;

  /* Response to notifiee's (ie, notify:'s and mnt-nfy:'s).
   * If we get here, then the transaction was a success.
   */
  init_response (tr, tmpfname, ti->sender_addrs, ti->notify_addrs, ti->nnext, 
		 notify_addrs, &nnext, nndx, &num_notify, NOTIFY_RESPONSE, ti,
                 db_admin);
  for (p = ti->notify_addrs; p  < ti->nnext; p += strlen (p) + 1) { 
    if ((j = present_in_list (notify_addrs, nnext, p)) < 0) {
      trace (NORM, tr, "ERROR: build_notify_responses() \"%s\" not in notify list\n", p);
      continue;
    }
    trace (NORM, tr, "notify: %s\n", p);
    notifier_response (tr, fin, fpos, ti, msg_hdl[nndx[j]].fp, 
		       max_obj_line_size);
  }
}

void send_email (trace_t *tr,  char *addrs, char *last, int ndx[], 
		 FILE *log_fp, int null_notification, int dump_stdout) {

  char buf[MAXLINE], *p;
  int i = 0;
 
  for (p = addrs; p < last; p += strlen (p) + 1, i++) {
    init_response_footer (msg_hdl[ndx[i]].fp);
    
    if (log_fp != NULL || dump_stdout) {

      if (!dump_stdout && log_fp != NULL)
	fprintf (log_fp, "\nMail to: \"%s\"\n", p);
      else if (dump_stdout && log_fp != NULL)
	fputs ("\nTCP reponse: \n", log_fp);
    
      if (fseek (msg_hdl[ndx[i]].fp, 0L, SEEK_SET) == EOF) {
	fputs ("ERROR: send_notifies() rewind seek error, skipping...\n", log_fp);
	trace (ERROR, tr, "send_notifies() feek error! (%s)\n", strerror(errno));
	continue;
      }
      
      /* write the sender response to the ack log */
      while (fgets (buf, MAXLINE, msg_hdl[ndx[i]].fp) != NULL) {
	if (log_fp != NULL)
	  fputs (buf, log_fp);

	if (dump_stdout) /* dump response to STDOUT */
	  printf ("%s", buf);

	trace (TR_TRACE, tr, "%s", buf);
      }
    }
  
    if (!null_notification && !dump_stdout && strcmp (p, UNKNOWN_USER_NAME)) { /* notify's to email */
      fflush (msg_hdl[ndx[i]].fp);
      
/*    chmod (msg_hdl[ndx[i]].fname, S_IRWXO|S_IRGRP|S_IRWXU); */
#ifdef HAVE_SENDMAIL
      sprintf (buf, "%s < %s", SENDMAIL_CMD, msg_hdl[ndx[i]].fname);
#elif HAVE_MAIL
      sprintf (buf, "%s \"%s\" < %s", MAIL_CMD, p, msg_hdl[ndx[i]].fname);
#else
      trace (NORM, tr, "No mail or sendmail found\n");
      trace (NORM, tr, "No response sent to (%s)\n", p);
      continue;
#endif
      trace (NORM, tr, "mail %s\n", p);
      if (system (buf) < 0) {
        trace (NORM, tr, "ERROR: send_email () \"%s\"\n", strerror (errno));
        break;
      }
    }
  }
}

/*
 * Send the notifications to the notifiee's.
 */
void send_notifies (trace_t *tr, int null_notification, FILE *ack_fp, int dump_stdout) {
  /*fprintf (dfile, "enter send_notifies ()...\n");*/

#if (defined(HAVE_SENDMAIL) || defined(HAVE_MAIL))
  send_email (tr, sender_addrs, snext, sndx, ack_fp, null_notification, dump_stdout);

  if (null_notification)
    return;

  send_email (tr, notify_addrs, nnext, nndx, NULL, 0, 0);
  send_email (tr, forward_addrs, fnext, fndx, NULL, 0, 0);
#else
  trace (NORM, tr, "*****No sendmail or mail binary found on system!***\n");
  trace (NORM, tr, "*****No notifications will be sent***\n");
#endif
}
