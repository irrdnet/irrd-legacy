/* Copyright 1998, Merit Network, Inc. and the University of Michigan */
/* $Id: util.c,v 1.3 2002/10/17 20:16:15 ljb Exp $
 * originally Id: util.c,v 1.3 1998/08/10 19:24:24 dogcow Exp  */

#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <fcntl.h>
#include <string.h>
#include <regex.h>
#include <stdlib.h>
#include <errno.h>
#include <ctype.h>
#include <sys/file.h>

#include <irrauth.h>

/*  check for HIDDENCRYPTPW magic value and copy over with old
 *  value if able to match up, return error if not
 */
int update_cryptpw (trace_t *tr, FILE *fd, long fpos, char *authinfo) {
  long savepos, curpos;
  char buf[MAXLINE], *q, *p, *r, *str_comment;
  int buflen, position = 0, count, len, found;

  savepos = ftell(fd);
  fseek (fd, fpos, SEEK_SET);

  while (fgets (buf, MAXLINE, fd) != NULL && buf[0] != '\n') {
    if (!is_auth(buf))
      continue;
    q = strchr(buf, ':');
    if (!q) /* sanity check */
      break;
    p = ++q;
    find_token (&p, &q);
    if (strncasecmp(p, "CRYPT-PW", 8))
      continue;
    position++;
    buflen = strlen(buf);
    find_token (&p, &q);
    if (strncmp(p, "HIDDENCRYPTPW", 13)) {
      continue;
    }
    if (p[13] != '\n')
      str_comment = p + 13;
    else
      str_comment = NULL;
    trace(INFO, tr, "update_cryptpw(): found HIDDENCRYPTPW - %s", buf);
    count = 1;
    found = 0;
    r = strtok(authinfo, "\n");
    while (r) {
      if (!strncasecmp(r, "CRYPT-PW", 8)) {
	r += 8;
	q = r;
	find_token (&r, &q);
	if (str_comment) {
	  q = r + 13;
	  len = strlen(q); 
	  if (!len)
	    continue;
	  trace(INFO, tr, "update_cryptpw(): matching comment field - %s", str_comment);
	  if (strncmp(str_comment, q, len))
	    continue;
	}
	trace(INFO, tr, "update_cryptpw(): copying old cryptpw - %s\n", r);
        found = 1;
	curpos = ftell(fd);
	fseek (fd, p - buf - buflen, SEEK_CUR);
	fwrite(r, 13, 1, fd);
	fseek (fd, curpos, SEEK_SET);
        if (count == position)	/* quit if we hit the same cryptpw position */
	  break;
      }
      count++;
      r = strtok(NULL, "\n");
    }
    if (!found) {
      fseek (fd, savepos, SEEK_SET);
      return -1;
    }
  }
  fseek (fd, savepos, SEEK_SET);
  return 0;
}

/* 
 * See if the object in file *fd1 is the same as the
 * object in file *fd2.
 *
 * Return:
 *   1 if the files are the same
 *   0 Otherwise
 */
int noop_check (trace_t *tr, FILE *fd1, long fpos1, FILE *fd2, long fpos2) {
  long offset1, offset2;
  int retval = 0;
  int overflow = 0;
  char line1[MAXLINE], line2[MAXLINE], *eof1, *eof2;

  if (fd1 == NULL || fd2 == NULL)
    return 0;

  offset1 = ftell (fd1);
  offset2 = ftell (fd2);
  fseek (fd1, fpos1, SEEK_SET);
  fseek (fd2, fpos2, SEEK_SET);
  while (1) {
    line1[MAXLINE - 1] = 0xff;
    line2[MAXLINE - 1] = 0xff;
    eof1 = fgets (line1, MAXLINE, fd1);
    eof2 = fgets (line2, MAXLINE, fd2);
    if (eof1 == NULL || eof2 == NULL) {
      retval = (eof1 == eof2);
      break;
    }
    if (strcmp (eof1, eof2)) {
      retval = 0;
      break;
    }
    /* stop at an empty line */
    if (*eof1 == '\n' && !overflow) {
      retval = 1;
      break;
    }
    if (line1[MAXLINE - 1] == 0x00 && line1[MAXLINE - 2] != '\n')
      overflow = 1;
    else
      overflow = 0;
  }

  /* restore file pos's */
  fseek (fd1, offset1, SEEK_SET);
  fseek (fd2, offset2, SEEK_SET);

  return retval;
}

/* Filter out elements in list1 that are present
 * in list2 and return the new list.  Routine assumes
 * that list1 will be replaced with the result, so invoke
 * like this, "list1 = filter_duplicates (list1, list2)".
 *
 * Return:
 *  char string of space seperated list1 elements not
 *  in list2.  Result could be NULL if all list1 elements
 *  are in list2.
 */
char *filter_duplicates (trace_t *tr, char *list1, char *list2) {
  char *ret_list = NULL;
  char *p, *q, *r, *s;
  char c, d;
  int match;

  /*fprintf (dfile, "\n---\nenter filter_duplicates ()...\n");*/
  if (list1 == NULL ||
      list2 == NULL)
    return list1;

  /*
  fprintf (dfile, "list1 (%s)\n", list1);
  fprintf (dfile, "list2 (%s)\n", list2);*/
  p = q = list1;
  while (find_token (&p, &q) > 0) {
    c = *q;
    *q = '\0';
    match = 0;
    r = s = list2;
    /*fprintf (dfile, "list1: %s\n", p);*/
    while (find_token (&r, &s) > 0) {
      d = *s;
      *s = '\0';
      /*fprintf (dfile, "list2: %s\n", r);*/
      if (!strcmp (p, r)) {
	/*fprintf (dfile, "match (%s)\n", r);*/
	match = 1;
	*s = d;
	break;
      }
      *s = d;
    }
    if (!match)
      ret_list = myconcat (ret_list, p);      
    *q = c;
  }

  free_mem (list1);

  /*if (ret_list != NULL)
  fprintf (dfile, "ret_list (%s)\n---\n", ret_list);
  else
  fprintf (dfile, "ret_list is NULL ie, all matches...\n---\n");
  */
  return ret_list;
}

/* Write the transaction object pointed to by fpos to
 * the output file.  The output file is assumed to be
 * in the correct position.  Also, function will trim
 * large objects (ie, # lines > max_line_size) if they
 * have errors (since they won't be used in an update).
 * Error output from irr_auth and irr_rpsl_check will not be
 * filtered.
 *   Filtering on large objects that will be sent to
 * IRRd (to avoid sending large objects in email) must 
 * be filtered in irr_notify, since irr_notify sends the 
 * updates to IRRd.
 *
 * Return:
 * void
 */
void write_trans_obj (trace_t *tr, FILE *fin, long fpos, FILE *fout, 
		      int max_line_size, int has_errors) {
  char buf[MAXLINE];
  int print_snip = 1;

  fseek (fin, fpos, SEEK_SET);
  while (fgets (buf, MAXLINE - 1, fin) != NULL) {
    if (buf[0] == '\n')
      break;
    if (has_errors &&
	--max_line_size < 0 && 	  
	!(is_changed (buf)       ||
	  is_source (buf)        ||
	  buf[0] == ERROR_TAG[0] ||
	  buf[0] == WARN_TAG[0])) {
      if (print_snip && max_line_size < 0) {
	fputs (SNIP_MSG, fout);
	print_snip = 0;
      }
      continue;
    }      
    
    if (EOF == fputs (buf, fout))
      fprintf (stderr, "ERROR: writing object to file (%s)\n", 
	       strerror (errno)); 
  }

  /* make sure the last line ends with a '\n' */
  if (buf[0] != '\n')
    fputs ("\n", fout);
}

char *pick_members (char *s) {
  char c, *p, *q, *r = NULL;

  p = q = s;
  while (find_token (&p, &q) > 0) {
    if (*(q - 1) == ',') {
      c = *(q - 1);
      *(q - 1) = '\0';
    }
    else {
      c = *q;
      *q = '\0';
    }
      
    r = myconcat (r, p);
    if (c == ',')
      *(q - 1) = ',';
    else
      *q = c;
  }

  return r;
}

/* 
 * Return a list of the "attr's" in a char string seperated by blanks.
 * Function assumes fpos pointer is at the beginning of the object.
 *
 * Return:
 *  List of attr values or NULL if none exist
 *  Restores the file position pointer on exit
 */
char *cull_attribute (trace_t *tr, FILE *fd, long obj_start, u_int attr) {
  char buf[MAXLINE];
  char *p = NULL, *q;
  int in_attr = 0;
  long fpos;
  int m=0, n=0;

  /* restore file pos upon exit */
  fpos = ftell (fd);

  /* seek the object start */
  if (fseek (fd, obj_start, SEEK_SET)) {
    fprintf (stderr, "cull_attribute () file seek error: (%s)\n", strerror(errno));
    exit (0);
  }

  /*fprintf (dfile, "\n---\nenter cull_attribute (attr %d) curr fpos (%ld) obj fpos (%ld)...\n", attr, fpos, obj_start);*/

  while (fgets (buf, MAXLINE - 1, fd) != NULL && buf[0] != '\n') {
    m++;
    if ((in_attr = find_attr (tr, buf, in_attr, attr, &q))) {
      n++;
      if (in_attr == AUTH_ATTR) {
        p = myconcat_nospace (p, q);
      } else
        p = myconcat (p, pick_members (q));
    }
  }

  /* restore prior file position */
  fseek (fd, fpos, SEEK_SET);

  /*
  fprintf (dfile, "cull_attr (): looked at (%d) lines found (%d) matches\n", m, n);
  if (p == NULL)
    fprintf (dfile, "cull_attr () returns empty list...\n---\n\n");
  else
  fprintf (dfile, "cull_attr () returns (%s)\n---\n\n", p);*/

  return p;
}

/* Given an attribute from a DB object return it's value part
 * if the attribute is a member of 'target_attrs' (ie, a bit
 * word of attributes to match).
 *
 * This routine is RPSL-capable, ie it handles rpsl-style 
 * line continuation, multiple addr's on a single line, 
 * and embedded comments.
 *
 * Return:
 *  ATTR_ID of attribute matched, attribute value is pointed
 *  to by **data.
 *  X_ATTR (ie, no match) if the attribute is not in 'target_attrs'.
 */
enum ATTR_ID find_attr (trace_t *tr, char *line, int in_attr, 
			u_int target_attrs, char **data) {
  char *p, *q;
  int line_cont = 0;

  *data = NULL;
  if (line[0] == '#')
    return (enum ATTR_ID) in_attr;

  /* handle RPSL line continuation */
  if (in_attr)
    in_attr = line_cont = (line[0] == ' ' || line[0] == '\t' || line[0] == '+');

  if (!in_attr) {
    if ((target_attrs & AUTH_ATTR) && is_auth (line))
      in_attr = AUTH_ATTR;
    else if ((target_attrs & MNT_NFY_ATTR) && is_mnt_nfy (line))
      in_attr = MNT_NFY_ATTR;
    else if ((target_attrs & NOTIFY_ATTR) && is_notify (line))
      in_attr = NOTIFY_ATTR;
    else if ((target_attrs & MNT_BY_ATTR) && is_mnt_by (line))
      in_attr = MNT_BY_ATTR;
    else if ((target_attrs & UPD_TO_ATTR) && is_upd_to (line))
      in_attr = UPD_TO_ATTR;
    else if ((target_attrs & MNTNER_ATTR) && is_mntner (line))
      in_attr = MNTNER_ATTR;
  }

  if (in_attr) {
    /* read past the attribute field label */
    p = q = line;
    if (!line_cont) {
      if ((q = strchr (line, ':')) == NULL)
	return X_ATTR;
      p = ++q;
    } else /* line cont, read past the first char, could be a '+' */
      p++;
    
    /* find the attr token  */
    find_token (&p, &q);

    if (in_attr != AUTH_ATTR) { /* don't remove comments from auth lines */
      if ((q = strchr (p, '#')) != NULL)
         *q = '\0';
      else
         newline_remove(line); /* remove newline from non-auth lines */
    }

    *data = p;
  }

  return (enum ATTR_ID) in_attr;
}

/* Open up a streams file.  If the file mode starts with 'w'
 * then routine makes up a name using the mkstemp () utility.
 * '*tag' should be an identifier string for error reporting
 * such as "maint_check () output file".
 *
 * Return:
 *  A stream file pointer if the operation was a success.
 *  NULL if the file could not be opened.
 */
FILE *myfopen (trace_t *tr, char *fname, char *fmode, char *tag) {
  FILE *fp;
  int fd;
  
  if (*fmode == 'w') {
    strcpy (fname, tmpfntmpl);
    fd = mkstemp(fname);
    if (fd == -1) {
      trace(ERROR, tr, "myfopen(): %s \"%s\": (%s)\n", tag, fname, strerror(errno));
      return(NULL);
    }
    fp = fdopen(fd, fmode);
    if (fp == NULL) {
      close(fd);
      unlink(fname);
    }
  } else
    fp = fopen(fname, fmode);
  
  if (fp == NULL)
     trace(ERROR, tr, "myfopen(): %s \"%s\": (%s)\n", tag, fname, strerror (errno));

  return fp;
}

/* Lock file 'filename'.  The lockfile name will not
 * be 'filename'.  The lockfile name will be 'filename.LOCK'.
 * This convention makes it easier for the log_roll () function.
 *
 * Return:
 *   -1 flock () error
 *    0 no errors, LOCK file is locked
 *    1 could not open the LOCK file (ie, fopen () fails)
 */ 
int lock_file (int *fd, char *filename) {
  char lockname[256];
  
  sprintf (lockname, "%s.LOCK", filename);
  if ((*fd = open (lockname, O_CREAT|O_WRONLY, 0644)) < 0)
    return 1;

#ifdef USE_FLOCK
  return flock (*fd, LOCK_EX);
#elif USE_LOCKF
  return lockf (*fd, F_LOCK, 0);
#else
  return 0;
#endif
}

/* Unlock and close the lock file pointed to by 'fp'.
 *
 * Return:
 *   void
 */
void unlock_file (int fd) {

#ifdef USE_FLOCK  
  flock (fd, LOCK_UN);
#elif USE_LOCKF
  lockf (fd, F_ULOCK, 0);
#endif
  close (fd);
}

/* Given a file name template, 'fntmpl' (eg, "%s/ack.log") and
 * a log directory path, 'log_dir', roll the current log onto 
 * the old log if the current log exceeds 'MAX_LOG_FILE_SIZE'.
 *
 * Return:
 *   void
 */
void log_roll (char *fntmpl, char *log_dir, int SKIP_LOCK) {
  struct stat stat_buf;
  char logfn[256], oldfn[256];
  int fd, lock_code = 0;
  int lfp;

  if (log_dir == NULL)
    return;

  sprintf (logfn, fntmpl, log_dir);
  if (!SKIP_LOCK) {
    if ((lock_code = lock_file (&lfp, logfn)) == 1)
      return;
  }

  /* no errors, got the lock */
  if (lock_code == 0) {
    if ((fd = open (logfn, O_RDONLY, 0774)) > 0) {
      fstat (fd, &stat_buf);
      close (fd);
      if (stat_buf.st_size > MAX_LOG_FILE_SIZE) {
	sprintf (oldfn, "%s.1", logfn);
	rename(logfn, oldfn);
      }
    }
    if (!SKIP_LOCK)
      unlock_file (lfp);
  }
  else /* lock error */
    close (lfp);
}
