/*
 * $Id: auth.c,v 1.9 2002/10/17 20:16:13 ljb Exp $
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <ctype.h>
#ifdef HAVE_CRYPT_H
#include <crypt.h>
#endif
#include <irrauth.h>

extern config_info_t ci;

#ifndef HAVE_CRYPT_H
extern char *crypt (const char *, const char*);
#endif

/*JW: need to add support for RPSL comments within an object */

/* local yokel's */

static void maint_check (trace_t *tr, FILE *, int *, int *, trans_info_t *);
static int determine_op_type (trace_t *tr, int *sockfd, FILE **fd_old, trans_info_t *ti, 
			      char *fname, int *num_trans, long *obj_pos);
static enum AUTH_CODE perform_auth_check (trace_t *tr, int *, int, FILE *, long, FILE *, 
					  long, trans_info_t *, int *);
static enum AUTH_CODE do_auth_check (trace_t *tr, char *, trans_info_t *);
static FILE *get_db_object (trace_t *tr, int *num_trans, char *fname, int *sockfd, 
			    int obj_size, char *obj_type, char *obj_key, char *source,
			    long *obj_pos);
static enum AUTH_CODE auth_process (trace_t *tr, int op, int *sockfd, FILE *fd, long fpos, 
				    trans_info_t *ti, int *num_trans, char **mb_list, 
				    char **mb_exist_list);
static enum AUTH_CODE mnt_by_exist (trace_t *tr, int *sockfd, trans_info_t *ti, 
				    char **mb_list, int *num_trans);
static enum AUTH_CODE auth_ok (trace_t *tr, int op, int *sockfd, trans_info_t *ti, 
			       char *mntner, int *mntner_exists, int *num_trans);
static int check_crypt_passwd (char *cleartxt, char *encrypted);
static void update_trans_info (trace_t *tr, enum AUTH_CODE ret_code, trans_info_t *ti,
			       int op, FILE *fd_new, long fpos, FILE *fd_old, 
			       long old_obj_pos);
static enum AUTH_CODE mntner_mblist_check (trace_t *tr, int op, int *sockfd, 
					   trans_info_t *ti, int *num_trans, 
					   char **mb_list, char **mb_exist_list);

/* String array's for debug output */
char *str_op[] = {"DEL", "ADD", "REPLACE"};
char *str_res[] = {"AUTH_FAIL_C", "AUTH_PASS_C", "OTHER_FAIL_C", 
		   "MNT_NO_EXIST_C", "DEL_NO_EXIST_C", "NOOP_C"};

/* do_auth_check () regex */
static char auth_val[] = "^[ \t]*([^ \t\n]+)[ \t]*([^ \t\n]+)?[ \t]*";
static regex_t authre;

/* Global object lookup structure
 * Briefly, our transaction file may have multiple
 * updates and so it may have the latest object
 * instance rather than IRRd.  This linked list will keep
 * track of object keys and file pointer to object starts in
 * the transaction file (ie, the output file this routine generates).
 */
lookup_info_t gstart;

/* Used by get_db_object().  Easier to decalare
 * global to this file as get_db_object() is nested
 * fairly deeply and frequently.
 */
static char *IRRd_HOST;
static int  IRRd_PORT;
static char *SUPER_PASSWD;

int auth_check (trace_t *tr, char *infname, char *outfname, 
		char *irrd_host, int irrd_port, char *super_passwd) {
  int num_trans = 0, sockfd;
  long fin_offset, fout_offset;
  char buf[MAXLINE];
  FILE *fin, *fout;
  trans_info_t ti;  

  IRRd_HOST    = irrd_host;
  IRRd_PORT    = irrd_port;
  SUPER_PASSWD = super_passwd;

  /* open the input and output files */
  if ((fin = myfopen (tr, infname, "r+", "auth_check () input file"))== NULL)
     return(-1);

  if ((fout = myfopen (tr, outfname, "w+", "auth_check () output file")) == NULL) {
    fclose(fin);
    return (-1);
  }

  /* initialize the transaction list lookup struct */
  gstart.first = gstart.last = NULL;

  /* regex for do_auth_check () */
  regcomp(&authre, auth_val, (REG_EXTENDED|REG_ICASE));

  while (fgets (buf, MAXLINE, fin) != NULL) {
    if (strncmp (HDR_START, buf, strlen (HDR_START) - 1))
      continue;

    if (parse_header (tr, fin, &fin_offset, &ti))
      trace (ERROR, tr, "auth_check () parse_header error!\n");
      /* JW need to take some logical course of action later:
       * return 0;  illegal hdr field found or EOF 
       * (ie, no object after hdr): want to construct legal hdr 
       * with "OTHERFAIL" initialized 
       */

    /* perform the actual maintainer checking */
    if (!ti.syntax_errors)
      maint_check (tr, fin, &sockfd, &num_trans, &ti);

    /* write the header info for this trans object */
    print_hdr_struct (fout, &ti);

    /* copy the current object after the header info */
    fout_offset = ftell (fout);
    write_trans_obj (tr, fin, fin_offset, fout, 
		     MAX_IRRD_OBJ_SIZE, update_has_errors (&ti));

    /* print a blank line seperator between objects */
    if (EOF == fputs ("\n", fout))
      trace (ERROR, tr, 
	     "auth () writing object to file (%s)\n", strerror (errno));

    /* update the transaction lookup list (see gstart) */
    trans_list_update (tr, &gstart, &ti, fout, fout_offset);

    free_ti_mem (&ti);
  }

  if (num_trans) {
    end_irrd_session (tr, sockfd);
    close_connection (sockfd);
    close (sockfd);
  }

  /* return trans list memory */
  free_trans_list (tr, &gstart);

  fflush (fout);
  fclose (fout);
  fclose (fin);
  return (1);
}

/* Contol the process of determining if the user has authorization to update 
 * the object.  The function determines the operation (ie ADD, DEL, REPLACE),
 * checks for update authorization and set's the notify/forward addr's for
 * notify.
 *
 * Return:
 *  (via the 'ti' struct)
 *  -notify/forward addr's
 *  -file with old object (for DEL and REPLACE op's)
 *  -authorization outcome
 */
void maint_check (trace_t *tr, FILE *fd_new, int *sockfd, int *num_trans, trans_info_t *ti) {
  int op;
  enum AUTH_CODE ret_code;
  FILE *fd_old = NULL;
  long fpos, old_obj_pos;
  char old_fname[256];
  char err_msg[256];

  /* save obj file pos to restore on exit */
  fpos = ftell (fd_new); 

  /* determine the operation type, ie, DEL, ADD, or REPLACE */
  if ((op = determine_op_type (tr, sockfd, &fd_old, ti, old_fname,
			       num_trans, &old_obj_pos)) < 0)
    return;

  if (*sockfd < 0) {
    ti->otherfail = strdup ("IRRd connection refused.  Abort transaction!");
    goto failout;
  }
  if(!ci.allow_inetdom && (!strcmp (ti->obj_type, "domain") || !strcmp(ti->obj_type,"inetnum") || !strcmp(ti->obj_type, "inet6num") || !strcmp(ti->obj_type,"as-block")) ) {
    sprintf(err_msg, "Registration of %s objects administratively prohibited\n", ti->obj_type);
    ti->otherfail = strdup (err_msg);
failout:
    if (fd_old != NULL) {
      fclose (fd_old);
      remove (old_fname);
    }
    return;
  }

  /* Perform the authorization check.  fd_new points to the DEL, ADD, 
   * or REPLACE object in the input file.  fd_old has a copy 
   * of the object to be DEL'd or REPLACE'd (for ADD fd_old is NULL).
   */
  ret_code = perform_auth_check (tr, sockfd, op, fd_old, old_obj_pos, fd_new, fpos, 
				 ti, num_trans);

  /* fold the transaction outcome/ret_code info into the object header struct */
  update_trans_info (tr, ret_code, ti, op, fd_new, fpos, fd_old, old_obj_pos);

  /* Notify will get: a file name  (ie, old object came from 
   * IRRd/DB) or a file pos (ie, a number; object came from our 
   * output/transaction file) for REPLACE and DEL operations.
   */
  if (fd_old != NULL) {
    if (old_obj_pos == 0L) {
      ti->old_obj_fname = strdup (old_fname);
      fclose (fd_old);
    }
    else {
      sprintf (old_fname, "%ld", old_obj_pos);
      ti->old_obj_fname = strdup (old_fname);
    }
  }

  /* restore fpos to the begining of object */
  fseek (fd_new, fpos, SEEK_SET);
}

/*
 * Determine the operation type, ie, ADD, DEL, or REPLACE.
 * If a delete field is provided by the user the OP: field
 * value will be set to DEL.  Otherwise the OP: field will 
 * be empty.
 */
int determine_op_type (trace_t *tr, int *sockfd, FILE **fd_old,
		trans_info_t *ti, char *fname, int *num_trans, long *obj_pos) {
  int op = -1;

  *fd_old = get_db_object (tr, num_trans, fname, sockfd, FULL_OBJECT,
			   ti->obj_type, ti->obj_key, ti->source, obj_pos);
  /* ADD or REPLACE */
  if (ti->op == NULL) {
    if (*fd_old == NULL) {
      op = iADD;
      set_op_type (ti->op, ADD_OP);
    }
    else {
      op = iREPLACE;
      set_op_type (ti->op, REPLACE_OP);
    }
  }
  else if (!strcmp (ti->op, DEL_OP))
    op = iDEL;
  else
    fprintf (stderr, "ERROR: determine_op_type () unrecognized op (%s)...\n", ti->op);

  /*fprintf (dfile, "return determine_op_type () (%d)\n", op);*/
  return op;
}

/* Control the process of determining authorization.  The function is
 * passed an 'op' (ie, DEL, ADD, REPLACE) and 1 or 2 non-empty object
 * files; an authorization code is returned.  On return the mnt_nfy 
 * list (AUTH_PASS_C) or the upd_to list (AUTH_FAIL_C) will be complete 
 * (ie, from all referenced maintainers).  It is the calling routines
 * onus to remove mnt_nfy/upd_to addr's based on the return code.
 *
 * Return:
 *   All legal 'enum AUTH_CODE' return codes are possible (see auth.h)  
 *   The 'ti' struct 'ti->forward' and 'ti->notify' may be updated.
 */
enum AUTH_CODE perform_auth_check (trace_t *tr, int *sockfd, int op, FILE *fd_old, 
				   long fpos_old, FILE *fd_new, long fpos_new, 
				   trans_info_t *ti, int *num_trans) {
  enum AUTH_CODE ret_code = AUTH_FAIL_C;
  char *mb_list = NULL, *mb_list2 = NULL, *mb_exist_list = NULL;
  
  /*fprintf (dfile, "\n---Enter perform_auth_check () op-(%s)...\n", str_op[op]);*/
  
  if (op != iADD) {
    if (fd_old == NULL)
      return DEL_NO_EXIST_C;
    
    if (op == iREPLACE && noop_check (tr, fd_old, fpos_old, fd_new, fpos_new))
      return NOOP_C;
    
    /* Determine authorization */
    if ((ret_code = auth_process (tr, iDEL, sockfd, fd_old, fpos_old, ti, num_trans, 
				  &mb_list, &mb_exist_list)) != AUTH_PASS_C)
      goto FREE_MEM;
  }
  
  if (op != iDEL) {
    if (op == iADD && /* Determine authorization */
	(ret_code = auth_process (tr, iADD, sockfd, fd_new, fpos_new, ti, num_trans, 
				  &mb_list2, &mb_exist_list)) != AUTH_PASS_C)
      goto FREE_MEM;

    /* Next section makes sure all new maintainer references exist */

    /* Remove possible self-mntner reference from new maint submission's */
    if (op == iADD       &&
	mb_list2 != NULL && 
	!strcmp (ti->obj_type, "mntner")) {
      /*fprintf (dfile, "remove possible maintainer reference from maint (%s) list (%s)\n", 
	ti->obj_key, mb_list2);*/
      mb_list2 = filter_duplicates (tr, mb_list2, ti->obj_key);
    }
    
    /* For speed up, remove maintainers found in authorization determination */
    if (op == iREPLACE && ti->mnt_by[0] != '\0') {
      mb_list2 = strdup (ti->mnt_by);
      /* filter out all mntner ref's in mb_list2 that are in mb_exist_list */
      mb_list2 = filter_duplicates (tr, mb_list2, mb_exist_list);
    }
    
    /* Maintainer references for new objects must exist */
    if ((ret_code = mnt_by_exist (tr, sockfd, ti, &mb_list2, num_trans)) != AUTH_PASS_C)
      goto FREE_MEM;
    
    mb_list = filter_duplicates (tr, mb_list, mb_list2);
  }
  
  /* For remaining members of mb_list (ie, DEL and REPLACE), get 
   * the maintainer and pick off "mnt-nfy" fields, add the new email 
   * addrs to ti->notify
   */
  if (mb_list != NULL) {
    mnt_by_exist (tr, sockfd, ti, &mb_list, num_trans);
    ti->maint_no_exist = free_mem (ti->maint_no_exist);
  }
  
FREE_MEM:
  free_mem (mb_list);
  free_mem (mb_list2);
  free_mem (mb_exist_list);

  /*fprintf (dfile, "perform_auth_check () returns-(%d)\n---\n\n", ret_code);*/
  return ret_code;
}

/* This routine collects all maintainer references collected from an ADD or REPLACE
 * object and check's if they exist.  For each referenced maintainer the
 * mnt-nfy's are collected in ti->notify.
 *
 * Return:
 *   AUTH_PASS_C if all mb_list members exist and ti->notify is appended
 *   with mnt_nfy's from the referenced maintainer's.
 *
 *   MNT_NO_EXIST_C if one or more of the mb_list members do not exist
 *   and are returned in ti->maint_no_exist.
 */
enum AUTH_CODE mnt_by_exist (trace_t *tr, int *sockfd, trans_info_t *ti, char **mb_list, 
			     int *num_trans) {
  char *p, *q, c;
  char fname[256], *ret_list = NULL, *mnt_nfy_list;
  long obj_pos;
  FILE *fd;

  /*
  if (*mb_list == NULL)
    fprintf (dfile, "enter mb_by_exist () mb_list is NULL\n");
  else
    fprintf (dfile, "enter mnt_by_exist (%s)\n", *mb_list);
  */

  p = q = *mb_list;
  while (find_token (&p, &q) > 0) {
    c = *q;
    *q = '\0';
    /*fprintf (dfile, "mnt_by_exist () see if (%s) exists\n", p);*/
    if ((fd = get_db_object (tr, num_trans, fname, sockfd, SHORT_OBJECT,
			     "mntner", p, ti->source, &obj_pos)) == NULL) {
	ret_list = myconcat (ret_list, p);
	/*fprintf (dfile, "mnt_by_exist () (%s) does not exist; aggregate list (%s)\n", p, ret_list);*/
    }
    else {
      if (ret_list == NULL) {
        mnt_nfy_list = cull_attribute (tr, fd, obj_pos, (u_int) MNT_NFY_ATTR);
        ti->notify = myconcat (ti->notify, mnt_nfy_list);

	/*if (mnt_nfy_list != NULL)
	  fprintf (dfile,"mnt_by_exist () (%s) exist's; adding notify (%s)\n", p, mnt_nfy_list);
	  if (ti->notify != NULL)
	  fprintf (dfile,"mnt_by_exist () (%s) aggregate notify list\n", ti->notify);*/

        free_mem (mnt_nfy_list);
      }
      /* don't delete objects in our output file */
      if (obj_pos == 0L) {
	fclose (fd);
	remove (fname);
      }
    }
    *q = c;
  }

  if (ret_list != NULL) {
    ti->maint_no_exist = myconcat (ti->maint_no_exist, ret_list);
    /*fprintf (dfile, " mnt_by_exist ret_code (MNT_NO_EXIST_C)\n");*/
    return  MNT_NO_EXIST_C;
  }
  /*fprintf (dfile, " mnt_by_exist ret_code (AUTH_PASS_C)\n");*/
  *mb_list = free_mem (*mb_list);
  return AUTH_PASS_C;
}

/* Control the process of determining if one of the maintainer's referenced
 * in this object passes authorization.
 *
 * Return:
 *   one of the 'enum AUTH_CODE's (see irrauth.h)
 */
enum AUTH_CODE auth_process (trace_t *tr, int op, int *sockfd, FILE *fd, long fpos, 
			     trans_info_t *ti, int *num_trans, char **mb_list,
                             char **mb_exist_list) {
  enum AUTH_CODE auth_ret;
  
  if (op == iADD) {
    if (ti->mnt_by != NULL)
      *mb_list = strdup (ti->mnt_by);
    else
      *mb_list = NULL;
  }
  else {
    *mb_list = cull_attribute (tr, fd, fpos, (u_int) MNT_BY_ATTR);
  } 

  if (ti->override) {
    if (check_crypt_passwd (ti->override, SUPER_PASSWD))
      return AUTH_PASS_C;
    else {
      trace (NORM, tr, "** Auth failure -- override password did not match\n");
      return BAD_OVERRIDE_C;
    }
  }

  /* bounce out maintainer add/delete requests unless you 
   * know the super user override password
   */
  if (op == iADD && !strcmp (ti->obj_type, "mntner"))
    return NEW_MNT_ERROR_C;

  if (op == iDEL && !strcmp (ti->obj_type, "mntner") && !strcmp(ti->op, DEL_OP) && *mb_list != NULL) {
    auth_ret = mntner_mblist_check (tr, op, sockfd, ti, num_trans, mb_list, mb_exist_list);
    if (auth_ret == AUTH_PASS_C)
      return DEL_MNT_ERROR_C;
    else
      return auth_ret;
  }
  
  if (*mb_list == NULL) {
    if (op == iADD) /* new submissions must have a valid maint */
      return AUTH_FAIL_C;
    else            /* if there are junk objects in the DB we must go along :( */
      return AUTH_PASS_C;
  }
  
  /* see if one of the referenced mntner's pass authorization */
  return mntner_mblist_check (tr, op, sockfd, ti, num_trans, mb_list, mb_exist_list);
}

/* Given a maintainer reference (*mntner), this function passes the 
 * auth: lines from the maintainer to do_auth_check () for authorization. 
 * mnt_nfy and upd_to fields are also collected in ti->notify and ti->forward.
 * 
 * Return:
 *  AUTH_PASS_C if authorisation passes.  The mny_nfy references from
 *  all the maintainers it took to establish authorisation (ie, some
 *  mnt_nfy ref's may still need to be collected) are appended to 'ti->notify'.
 *
 *  AUTH_FAIL_C if authorisation could not be obtained.  In this case
 *  ti->forward will have all upd_to: ref's.
 *
 *  MNT_NO_EXIST_C if the referenced maintainer does not exist.
 */
enum AUTH_CODE auth_ok (trace_t *tr, int op, int *sockfd, trans_info_t *ti, 
			char *mntner, int *mntner_exists, int *num_trans) {
  int in_attr = 0, skip = 0;
  long obj_pos, fpos;
  FILE *fd;
  enum AUTH_CODE ret_code = AUTH_FAIL_C;
  char buf[MAXLINE], fname[256], *q;

  if ((fd = get_db_object (tr, num_trans, fname, sockfd, SHORT_OBJECT,
			   "mntner", mntner, ti->source, &obj_pos)) == NULL) {
    trace (NORM, tr, "Maintainer (%s) does not exist. Authorization fails!\n", mntner);
    *mntner_exists = 0;
    if (op == iADD)
      return MNT_NO_EXIST_C;
    else
      return AUTH_FAIL_C;
  }
  else {
    /*fprintf (dfile, "auth_ok () say's (%s) exists, so gather notify's/auth: lines...\n", mntner);*/
    *mntner_exists = 1;
    /* save the fpos to restore on exit */
    fpos = ftell (fd);
    fseek (fd, obj_pos, SEEK_SET);

    while (fgets (buf, MAXLINE, fd) != NULL && buf[0] != '\n') {
      if ((in_attr = find_attr (tr, buf, in_attr, 
				(u_int) (AUTH_ATTR|MNT_NFY_ATTR|UPD_TO_ATTR), &q))) {
	trace (NORM, tr, "auth_ok () find_attr: (%s)\n", q);
	if (in_attr == AUTH_ATTR) {
	  if (!skip && (ret_code = do_auth_check (tr, q, ti)) == AUTH_PASS_C)
	    skip = 1;
	}
	else if (in_attr == MNT_NFY_ATTR)
	  ti->notify = myconcat (ti->notify, q);
	else
	  ti->forward = myconcat (ti->forward, q);
      }
    }

    /* JW later it would be better to keep file around till the
     * end for possible reuse */
    /* ie, don't close and remove our output file */
    if (obj_pos == 0L) {
      fclose (fd);
      remove (fname);
    }
    else
      fseek (fd, fpos, SEEK_SET);
  }

  /* fprintf (dfile, "auth_ok () ret_code-(%d)\n", ret_code);*/
  return ret_code;
}

/* Place a copy of the referenced DB object into file 'fname' should the 
 * DB object exist.  The object can come from our transaction output file
 * (latest copy) or from IRRd.  'obj_size' can be FULL_OBJECT or SHORT_OBJECT.
 * SHORT_OJBECT will return only auth, mnt_nfy, mb_by, and notify fields.
 *
 * Return:
 *  Stream file pointer to the object if it was retrieved successfully
 *  and the name of the file in 'fname'.
 *  NULL otherwise.
 */
FILE *get_db_object (trace_t *tr, int *num_trans, char *fname, int *sockfd, 
		     int obj_size, char *obj_type, char *obj_key, 
		     char *source, long *obj_pos) {
  obj_lookup_t *obj;

  /*fprintf (dfile, "\n---\nenter get_db_object () key-(%s)\n", obj_key);*/

  /* see if the object is in our trans obj list first */
  if ((obj = find_trans_object (tr, &gstart, source, obj_type, obj_key)) != NULL) {
    /* object has been deleted */
    if (obj->state == 0)
      return NULL;
    else {
      /*fprintf (dfile, "get_db_object () found trans obj (%s) fpos (%ld)\n", obj_key, obj->fpos);*/
      *obj_pos = obj->fpos;
      return obj->fd;
    }
  }

  /* The object was not in our transaction list, get it from IRRd. */
  *obj_pos = 0L;
  strcpy (fname, tmpfntmpl);
  return IRRd_fetch_obj (tr, fname, num_trans, sockfd, obj_size,
			 obj_type, obj_key, source, MAX_IRRD_OBJ_SIZE,
			 IRRd_HOST, IRRd_PORT);
}

enum AUTH_CODE do_auth_check (trace_t *tr, char *auth_line, trans_info_t *ti) {
  char *p, *q;
  int auth_tokens;
  regex_t re;
  regmatch_t authrm[3];
  enum AUTH_CODE ret_code;

  /*fprintf (dfile, "do_auth_check () found an auth token-(%s)\n", auth_line);*/
  /* See if the line is a well-formed 'auth: ...' attr */
  if (0 != regexec (&authre, auth_line, 3, authrm, 0))
    return AUTH_FAIL_C;
  
  /* first token in 'auth:' attr */
  p = auth_line + authrm[1].rm_so;
  *(auth_line + authrm[1].rm_eo) = 0;

  /* see if there second token */
  if (authrm[2].rm_so != -1)
    auth_tokens = 2;
  else
    auth_tokens = 1;

  /* PGPKEY-XXXXXXXX auth check */
  if (!strncasecmp ("PGPKEY-", p, 7)) {
    if (auth_tokens == 1    &&
	ti->pgp_key != NULL &&
	/* the PGP key from PGP does not have the 'PGPKEY-' prefix
	 * so we compare the hexid part of the PGPKEY-(xxxxxxxx) only
	 * to the hexid of from the PGPV output */
	!strncasecmp (ti->pgp_key, p + 7, 8))
      return AUTH_PASS_C;
    return AUTH_FAIL_C;
  }
 

#ifdef ALLOW_AUTH_NONE 
  /* NONE auth check - this is deprecated */
  if (!strncasecmp ("NONE", p, 4)) {
    /*fprintf (dfile, "do_auth_check () auth NONE!\n");*/
    if (auth_tokens == 1)
      return AUTH_PASS_C;
    return AUTH_FAIL_C;
  }
#endif

  /* This is bad if this test is true */
  if (auth_tokens == 1)
    return AUTH_FAIL_C;

  /* second token in 'auth:' attr */
  q = auth_line + authrm[2].rm_so;
  *(auth_line + authrm[2].rm_eo) = 0;

  /* assume the worst :) */
  ret_code = AUTH_FAIL_C;

  /* MAIL-FROM auth check */
  if (!strncasecmp ("MAIL-FROM", p, 9) && ti->sender_addrs != NULL) {
    /*fprintf (dfile, "do_auth_check () from-(%s) auth from-(%s)\n", ti->sender_addrs, p);*/
    if (!regcomp (&re, q, REG_EXTENDED|REG_NOSUB|REG_ICASE)) {
      if (!regexec(&re, ti->sender_addrs, (size_t) 0, NULL, 0))
	ret_code = AUTH_PASS_C;
      regfree (&re);
    }
    return ret_code;
  }

  /* CRYPT-PW auth check */
  if (!strncasecmp ("CRYPT-PW", p, 8) && ti->crypt_pw != NULL) {
    trace (NORM, tr, "do_auth_check () cleartxt passwd-(%s) crypt passwd-(%s)\n",ti->crypt_pw, q);
    if (check_crypt_passwd (ti->crypt_pw, q))
      return AUTH_PASS_C;
    return AUTH_FAIL_C;
  }
 
  /* We are in bad shape if we get here */
  return AUTH_FAIL_C;
}

/* Determine if the cleartxt password matches the encrypted password.
 * 
 * Return:
 *   1 if cleartxt password is a match
 *   0 otherwise
 */
int check_crypt_passwd (char *cleartxt, char *encrypted) {

  if (cleartxt == NULL ||
      encrypted == NULL)
    return 0;

  return !strcmp ((char *) crypt (cleartxt, encrypted), encrypted);
}

/* Fold the transaction outcome into the 'ti' struct.  The 'ti' struct
 * info will be translated into object header text in the output file for 
 * notify.  This function also performs the final notify/forward address
 * processing (which is dependent on the transaction outcome/'ret_code').
 *
 * Return:
 *  void
 */
void update_trans_info (trace_t *tr, enum AUTH_CODE ret_code, trans_info_t *ti,
			int op, FILE *fd_new, long fpos, FILE *fd_old, long old_obj_pos) {
  char *obj_notifies;
  char *authinfo = NULL;

  if (ret_code == AUTH_PASS_C) {
    /*fprintf (dfile, ">>>>>>>enter ret_code AUTH_PASS_C process...\n");*/
    if (op != iDEL) { /* JW can take out of this works &&
			 (obj_notifies = cull_attribute (dfile, fd_new, fpos, 
			 (u_int) (NOTIFY_ATTR|MNT_NFY_ATTR))) != NULL) {
			 ti->notify = myconcat (ti->notify, obj_notifies);
			 free_mem (obj_notifies);
			 */
      ti->notify = myconcat (ti->notify, ti->check_notify);
      ti->notify = myconcat (ti->notify, ti->check_mnt_nfy);
    }
    /* fprintf (dfile, "checking old/del object for notify's\n");*/
    if (fd_old != NULL &&
	(obj_notifies = cull_attribute (tr, fd_old, old_obj_pos, 
					(u_int) NOTIFY_ATTR)) != NULL) {
      ti->notify = myconcat (ti->notify, obj_notifies);
      free_mem (obj_notifies);
    }
    if (op == iREPLACE && !strcmp(ti->obj_type, "mntner")) {
      authinfo = cull_attribute(tr, fd_old, old_obj_pos, (u_int) AUTH_ATTR);
      /* need to check if HIDDENCRYPTPW is in new mntner object */
      if (authinfo != NULL) {
        trace (NORM, tr, "update_trans_info() auth attrs -- %s\n", authinfo);
        if (update_cryptpw (tr, fd_new, fpos, authinfo) != 0)
	  ti->otherfail = strdup("Unable to match HIDDENCRYPTPW!\n");
	free(authinfo);
      }
    }
    /*if (ti->notify != NULL)
      fprintf (dfile, "ret_code AUTH_PASS_C notify list (%s)\n", ti->notify);*/
  }
  else if (ret_code == DEL_NO_EXIST_C)
    ti->del_no_exist = 1;
  else if (ret_code ==  NOOP_C) {
    set_op_type (ti->op, NOOP_OP);
  }
  else if (ret_code == AUTH_FAIL_C) {
    ti->authfail = 1;
    /* Don't notify upd-to's on ADD operations */
    if (op == iADD)
      ti->forward = free_mem (ti->forward);
  }
  else if (ret_code == NEW_MNT_ERROR_C)
    ti->new_mnt_error = 1;
  else if (ret_code == DEL_MNT_ERROR_C)
    ti->del_mnt_error = 1;
  else if (ret_code == BAD_OVERRIDE_C)
    ti->bad_override = 1;
  
  if (CLEAR_NOTIFY & ret_code)
    ti->notify = free_mem (ti->notify);

  if (CLEAR_FORWARD & ret_code)
    ti->forward = free_mem (ti->forward);

  /* Bookeeping: remove trans fields not needed by notify */
  if (ret_code != BAD_OVERRIDE_C)
    ti->override    = free_mem (ti->override);
  ti->check_notify  = free_mem (ti->check_notify);
  ti->check_mnt_nfy = free_mem (ti->check_mnt_nfy);
}

/* Given a mnt_by: list from an object, control the process of
 * determining if any of the referenced maintainers pass authorization.
 * 
 * Return:
 *   One of the legal 'enum AUTH_CODE's
 *   Function will also remove the mntner references from 'mb_list'
 *   that were checked in the process of determining authorization.
 *   Remaining members of 'mb_list' will need to be checked for mnt_nfy
 *   references (when authorization passes).  If authorization fails, 
 *   all referenced 'upd_to:'s will have been gathered in 'ti->forward'.
 */
enum AUTH_CODE mntner_mblist_check (trace_t *tr, int op, int *sockfd, trans_info_t *ti, 
				    int *num_trans, char **mb_list, char **mb_exist_list) {
  char *p, *q, c;
  int mntner_exists;
  enum AUTH_CODE ret_code = AUTH_FAIL_C;
  
  /* see if any of the referenced maintainers pass auth */
  p = q = *mb_list;
  while (find_token (&p, &q) > 0) {
    c = *q;
    *q = '\0';
    /*fprintf (dfile, "auth_process () check maint-(%s)\n", p);*/
    ret_code = auth_ok (tr, op, sockfd, ti, p, &mntner_exists, num_trans); 
    /*fprintf (dfile, "auth_process () auth_ok ret_code-(%d)\n", ret_code);*/
    
    /* Save mntner's that exist to avoid expensive duplicate lookups */
    if (mntner_exists)
      *mb_exist_list = myconcat (*mb_exist_list, p);
    
    if (ret_code == MNT_NO_EXIST_C) {
      if (op == iADD) {
	ti->maint_no_exist = myconcat (ti->maint_no_exist, p);
	break;
      }
      ret_code = AUTH_FAIL_C;
    }
    
    *q = c;
    
    /* trim down the mb_list for later gathering of mnt_nfy's */
    if (ret_code == AUTH_PASS_C) {
      if (find_token (&p, &q) > 0)
	q = strdup (p);
      else
	q = NULL;
      free_mem (*mb_list);
      *mb_list = q;
      break;
    }
  }

  return ret_code;
}
