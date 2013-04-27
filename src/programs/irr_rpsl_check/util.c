#include <stdio.h>
#include <sys/types.h>
#include <stdarg.h>
#include <regex.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>

#include <irr_rpsl_check.h>
#include <irr_defs.h>

extern int verbose;

extern short attr_is_key[];
extern char *attr_name[];
extern char *obj_type[MAX_OBJS];
extern short legal_attrs[MAX_OBJS][MAX_ATTRS];
extern short mult_attrs[MAX_OBJS][MAX_ATTRS];
extern short mand_attrs[MAX_OBJS][MAX_MANDS];
extern char error_buf[MAX_ERROR_BUF_SIZE];
extern char *too_many_errors;
extern int too_many_size;
extern regex_t re[];
extern char parse_buf[];
extern int QUICK_CHECK_FLAG;
extern FILE *dfile;
extern char string_buf[];
extern int int_size;
extern int start_of_object;

int ERROR_BUFFER_FULL = 0;

void check_object_end (parse_info_t *obj, canon_info_t *canon_info) {
  char buf[MAXLINE], *estr = "errors", *wstr = "warnings";
  int i, j, type = obj->type;
  int skip_checks = 0;

  if (obj->num_lines == 0) {
    reset_token_buffer ();
    start_new_line = 1;
    return;
  }

  if (type == NO_OBJECT) {
    error_msg_queue (obj, "No key field specified", INFO_MSG);
    skip_checks = 1;
  }

  if (type >= MAX_OBJS) {
    error_msg_queue (obj, "Internal object type range error", INFO_MSG);
    skip_checks = 1;
  }

  if (!skip_checks) {
    if (verbose) fprintf (dfile, "checking for legal fields (type=%s)...\n", obj_type[type]);
    /* first we'll check for legal fields */
    /*    obj->curr_attr = F_NOATTR; ensure this message gets printed */
    for (i = 0; i < MAX_ATTRS; i++)
      if (obj->attrs[i] > 0 && !legal_attrs[type][i]) {
	sprintf (buf, "end: Illegal attribute \"%s\"",  attr_name[i]);
	error_msg_queue (obj, buf, INFO_MSG);
      }

    /*if (verbose) fprintf (dfile, "checking for mult fields...\n");*/
    /* next we check for multiple fields */
    for (i = 0; i < MAX_ATTRS; i++)
      if (obj->attrs[i] > 1 && !mult_attrs[type][i]) {
	sprintf (buf, "Multiple \"%s\" fields not allowed", attr_name[i]);
	error_msg_queue (obj, buf, INFO_MSG);
      }

    /*if (verbose) fprintf (dfile, "checking for mando fields...\n"); */
    /* finally check the mandatory fields */
    for (i = 0; (j = mand_attrs[type][i]) >= 0; i++)
      if (obj->attrs[j] == 0) {
	sprintf (buf, "Mandatory field \"%s\" missing",  attr_name[j]);
	error_msg_queue (obj, buf, INFO_MSG);
      }
  }
    
  /* for 'key-cert' objects add the machine generated fields */
  if (!obj->errors && obj->type == O_KC)
    add_machine_gen_attrs (obj, canon_info);
  
  /*if (verbose) fprintf (dfile, "calling display_canonicalized_object()...\n");*/
  /* Show the user his/her nice shiney new object.
   * Also prepend the header information if we are part of
   * a pipeline of processing.
   */
  display_canonicalized_object (obj, canon_info);

  canon_info->num_objs++;
  if (obj->errors || obj->warns) {
    report_errors (obj);

    if (QUICK_CHECK_FLAG) {
      if (obj->errors == 1) 
	estr = "error";
      if (obj->warns == 1)  
	wstr = "warning";

      fprintf (ofile, "Found %d %s and %d %s, exit syntax check!\n", 
	       obj->errors, estr, obj->warns, wstr); 
	fprintf (ofile, "Number of objects checked: %d \n", canon_info->num_objs);
      exit (0);
    }
  }

  /* print a blank line to act as a seperator between objects */
  if (!QUICK_CHECK_FLAG)
    fprintf (ofile, "\n");

  start_new_object (obj, canon_info);
}

/* Add the machine generated attributes 'method:', 'fingerpr:',
 * and 'owner:' for 'key-cert' objects.  'owner:' is grabbed in
 * hexid_check () and 'fingerpr:' is grabbed in get_fingerprint ().
 * 'method:' can only have one value at this time (7/1/00).
 *
 * Return:
 *
 *   void
 */
void add_machine_gen_attrs (parse_info_t *pi, canon_info_t *ci) {
  char *p;

  /* sanity checks */
  if (pi->u.kc.fingerpr == NULL) {
    error_msg_queue (pi, "Internal error.  Couldn't find key fingerprint", INFO_MSG);
    return;
  }

  if (pi->u.kc.owner == NULL) {
    error_msg_queue (pi, "Internal error.  Couldn't find PGP owner", INFO_MSG);
    return;
  }

  /* add method */

  pi->curr_attr = F_MH;
  canonicalize_key_attr (pi, ci, 0);
  parse_buf_add (ci, "%s\n%z", "PGP");
  pi->num_lines++;
  pi->start_lineno++;
  start_new_canonical_line (ci, pi);

  /* add fingerprint */

  pi->curr_attr = F_FP;
  canonicalize_key_attr (pi, ci, 0);
  parse_buf_add (ci, "%s\n%z", pi->u.kc.fingerpr);
  pi->num_lines++;
  pi->start_lineno++;
  start_new_canonical_line (ci, pi);

  /* add owner(s) */

  p = strtok (pi->u.kc.owner, "\n");
  while (p != NULL) {
    pi->curr_attr = F_OW;
    canonicalize_key_attr (pi, ci, 0);
    parse_buf_add (ci, "%s\n%z", p);
    pi->num_lines++;
    pi->start_lineno++;
    start_new_canonical_line (ci, pi);
    p = strtok (NULL, "\n");
  }
}

void free_hdr_mem (parse_info_t *hi) {
  if (hi->op != NULL)
    free (hi->op);
  
  if (hi->obj_key != NULL)
    free (hi->obj_key);
  
  if (hi->source != NULL)
    free (hi->source);
  
  if (hi->mnt_by != NULL)
    free (hi->mnt_by);  
}

void start_new_object (parse_info_t *obj, canon_info_t *canon_info) {

fprintf (dfile, "\nstarting new object....\n");
  free_hdr_mem (obj);
  if (obj->union_type != EMPTY) {
    switch (obj->union_type) {
    case KEY_CERT: if (obj->u.kc.owner    != NULL)  free (obj->u.kc.owner);
                   if (obj->u.kc.certif   != NULL)  free (obj->u.kc.certif);
		   if (obj->u.kc.fingerpr != NULL)  free (obj->u.kc.fingerpr);
    case EMPTY:
      break;
    }
  }
  memset ((char *) obj, 0, sizeof (*obj));
  obj->curr_attr    = F_NOATTR;
  obj->type         = NO_OBJECT;
  obj->attr_error   = LEGAL_LINE;
  obj->err_msg      = obj->errp = error_buf;
  *(obj->err_msg)   = '\0';
  obj->start_lineno = 1;
  ERROR_BUFFER_FULL = 0;
  start_of_object   = 1;
  start_new_line    = 1;


  /* lineptr info */
  lineptr[0].attr = F_NOATTR;
  lineptr[0].count = 0;
  lineptr[0].ptr = parse_buf;

  canon_info->io = canon_info->lio = CANON_MEM; /* assume object can fit in memory */
  canon_info->buf_space_left = MAX_CANON_BUF_SIZE;
  canon_info->linep = canon_info->bufp = parse_buf;
  if (canon_info->fd != NULL) {
    fclose (canon_info->fd);
    canon_info->fd = NULL;
    unlink (canon_info->flushfn);
  }
  if (canon_info->lfd != NULL) {
    fclose (canon_info->lfd);
    canon_info->lfd = NULL;
    unlink (canon_info->lflushfn);
  }
    
  /*if (verbose) fprintf (dfile, "start_new_object (sizeof(obj)-(%d))\n",sizeof (*obj));*/
}

int irrcheck_find_token (char **x, char **y) {
  /*if (verbose) fprintf (dfile, "enter irrcheck_find_token(%s)...\n",*x);*/
  *x = *y;
  /* fprintf (dfile, "p-(%c)\n",**x); */
  while (**x != '\0' && **x != '\n' && (**x == ' ' || **x == '\t')) (*x)++;

  if (**x == '\0' || **x == '\n')
    return -1;

  *y = *x + 1;
  while (**y != '\0' && isgraph((int)**y)) (*y)++;

  /* fprintf (dfile, "irrcheck_find_token () returns 1..\n"); */
  return 1;  
}

/* Error message format:
 * 
 * #ERROR*: 4: tech-c:  "...."
 *  int     int    int  string
 * 
 * field 1: 1 = ERROR, 2 = WARNING
 * fiels 2: line number
 * field 3: F_MB, F_TC, ...
 * field 4: Error or warning message ('\0' terminated string)
 *
 * routine can also do message wrapping to continue
 * in a neat way on the next line for long messages.
 * This could be configurable.
 *
 * eg,
 *
 * error_msg_queue (ERR_MSG, F_TC, obj->lineno, "....");
 *
 * #define ERR_MSG  1
 * #define WARN_MSG 2
 */
void error_msg_queue (parse_info_t *obj, char *emsg, int msg_type) {
  char buf[MAXLINE];
  size_t n = sizeof (obj->curr_attr);
  size_t m = sizeof (msg_type);
  int slen;
  int sv;

  if (msg_type == ERROR_MSG || msg_type == ERROR_OVERRIDE_MSG)
    obj->attr_error = SYNTAX_ERROR;
  else if (msg_type == EMPTY_ATTR_MSG)
    obj->attr_error = EMPTY_LINE;

  /* Once the user has accumulated 1K's worth of error msgs
   * that is enough.
   */
  if (ERROR_BUFFER_FULL)
    return;

  /* make the error message */
  if (msg_type == INFO_MSG)
    snprintf (buf, MAXLINE, "%s%s\n", ERROR_TAG, emsg);
  else
    snprintf (buf, MAXLINE, "%s%d: %s: %s\n", 
	     ((msg_type == WARN_MSG || msg_type == WARN_OVERRIDE_MSG ||
	       msg_type == EMPTY_ATTR_MSG) ? WARN_TAG : ERROR_TAG),
	     obj->start_lineno + obj->elines - 1,
	     ((obj->curr_attr == F_NOATTR) ? "?" : attr_name[obj->curr_attr]),
	     emsg);

  slen = strlen (buf);
  /* here is error message CHL */

  /* Our error message buffer is sated */
  if (((obj->errp - obj->err_msg) + slen + 2*n + 2*m + too_many_size + 2) 
      >= MAX_ERROR_BUF_SIZE) {
    sv = obj->curr_attr;
    msg_type = INFO_MSG;
    memcpy (obj->errp, &msg_type, m);
    obj->errp += m;
    obj->curr_attr = F_NOATTR; /* this causes the message to be printed */
    memcpy (obj->errp, &(obj->curr_attr), n);
    obj->errp += n;
    strcpy (obj->errp, too_many_errors);
    obj->errp += strlen (too_many_errors) + 1;
    obj->curr_attr = sv;
    ERROR_BUFFER_FULL = 1;
    return;
  }

  /* Copy the error message to the error buffer */
  memcpy (obj->errp, &msg_type, m);
  obj->errp += m;
  memcpy (obj->errp, &(obj->curr_attr), n);
  obj->errp += n;
  strcpy (obj->errp, buf);
  obj->errp += slen + 1; /* want each message terminated with a '\0' */

  if (verbose) 
    fprintf (dfile, "error_msg_queue(), copied n bytes-(%d) curr attr-(%d)\n",(int) n, obj->curr_attr); 
  if (verbose) fprintf (dfile, "error_msg_queue(), msg-(%s)\n",emsg);
  if (msg_type == WARN_MSG          ||
      msg_type == WARN_OVERRIDE_MSG ||
      msg_type == EMPTY_ATTR_MSG)
    obj->warns++;
  else /* must be an error message */
    obj->errors++;
}

void report_errors (parse_info_t *obj) {
  short attr;
  char *ebuf, *p;
  size_t n = sizeof (obj->type);
  int msg_type;
  size_t m = sizeof (msg_type);

  ebuf = obj->err_msg;
  while (obj->errp > ebuf) {
    memcpy (&msg_type, ebuf, m);
    ebuf += m;
    memcpy (&attr, ebuf, n);
    ebuf += n;
/*
if (verbose) fprintf (dfile, "\nreport_errors (attr-(%d))\n",attr);
if (verbose) fprintf (dfile, "report_errors (emsg-(%s))\n",ebuf);
*/
    /* Want to suppress syntax errors on illegal attributes,
     * a single msg "illegal attr" is enough. 
     */
    if (msg_type == EMPTY_ATTR_MSG) {
      if (verbose) 
	fprintf (dfile, "report errors () attr (%s) empty line (%s)\n", attr_name[attr], ebuf);
      if (!obj->errors)
	fprintf (ofile, "%s", ebuf);
      else {
	if (obj->type != NO_OBJECT &&
	    attr      != F_NOATTR  &&
	    !legal_attrs[obj->type][attr]) {
	  obj->warns--;
	  obj->errors++;
	  fprintf (ofile, "%sIllegal attribute \"%s\"\n",  ERROR_TAG, attr_name[attr]);
	}
	/* want an empty attribute msg when empty field is a key field
	 * or legal field
	 */
	else if ((p = strrchr (ebuf, ':')) != NULL) {
	  *p = '\0';
	  fprintf (ofile, "%s: Empty attribute\n", ebuf);
	  *p = ':';
	}
      }
    }
    else if (msg_type == INFO_MSG           ||
	     msg_type == ERROR_OVERRIDE_MSG ||
	     msg_type == WARN_OVERRIDE_MSG  ||
	     obj->type == NO_OBJECT         ||
	     attr == F_NOATTR               ||
	     legal_attrs[obj->type][attr])
      fprintf (ofile, "%s", ebuf);
    else
      obj->errors--;

    ebuf = strchr (ebuf, '\0') + 1;
  }
}

/*
 * See if a given source DB exists.
 * If yes return pointer into sources list.
 * else return NULL.
 */
source_t *is_sourcedb (char *source_name, source_t *j) {
  source_t *i;

  for (i = j; i != NULL; i = i->next) {
    if (!strcasecmp (source_name, i->source))
      break;
  }
  return i;
}

/* Determine if '*s' is an RPSL reserved word.
 * 
 * Return:
 *
 * -1 if '*s' is not a reserved word
 * reserved word token value otherwise
 */
int is_reserved_word (char *s) {
  int i;

  if (s == NULL)
    return -1;
  
  for (i = 0; i < MAX_RESERVED_WORDS; i++)
    if (!strcasecmp (s, reserved_word[i]))
      return reserved_word_token[i];
  
  return -1;
}

/* Determine if '*s" starts with an RPSL reserved prefix.
 *
 * Return:
 *
 * 1 if '*s' begins with a reserved prefix
 * 0 otherwise
 */
int has_reserved_prefix (char *s) {
  int i;
  
  if (s == NULL)
    return -1;
  
  for (i = 0; i < MAX_RESERVED_PREFIXES; i++)
    if (!strncasecmp (s, reserved_prefix[i], strlen (reserved_prefix[i])))
      return 1;
  
  return 0;
}

/* Determine if '*s' is an RP (ie, routing policy) token.
 *
 * Return:
 *
 * 0 if '*s' is not a RP word
 * 1 otherwise
 */
rp_attr_t *is_RPattr(rp_attr_t_ll *ll, char *s) {
  rp_attr_t *p;

  if (s == NULL)
    return NULL;

/*  for (p = ll->first; p != NULL; p = p->next)
    fprintf (dfile, "method (%s)\n", p->name); */
  
  for (p = ll->first; p != NULL; p = p->next)
    if (!strcasecmp (s, p->name))
      break;

  return p;
}

method_t *find_method (method_t *first, char *name) {

  if (name == NULL)
    return NULL;

  for (; first != NULL; first = first->next)
    if (!strcasecmp (name, first->name))
      break;

  return first;
}

/*
 * Check for the special nic suffix's that don't
 * fit into the normal categories.
 */
int is_special_suffix (char *p) {
  if (!strcmp (p, "CC-AU") ||
      !strcmp (p, "1-AU")  ||
      !strcmp (p, "2-AU")  ||
      !strcmp (p, "ORG")   ||
      !strcmp (p, "AP")    ||
      !strcmp (p, "NOC"))
    return 1;

  return 0;
}

/*
 * The countries are derived from the rpsl.config
 * and are two letter uppercase abbreviations.  They
 * are used as a check for valid nic handle suffix's
 */
int is_country (char *p, char *countries[]) {
  int i;

  for (i = 0; i < MAX_COUNTRIES; i++)
    if (!strcmp (p, countries[i]))
      return i;

  return -1;
}

/* Given the number of variable arguments, 'num_args',
 * and the var args not to free, 'skipfree', concat
 * all the variable args together and return a new string.
 * Args not in 'skipfree' will be freed/returned to the 
 * system.
 *
 * Return:
 *   A string with all the variable args concatenated.
 */
char *my_strcat (parse_info_t *o, int num_args, u_int skipfree, ...) {
  char buf[MAX_ATTR_SIZE], *p;
  va_list ap;
  int i, j = 0, k = 0, len;
  u_int n = 1;

  va_start (ap, skipfree);
  for (i = num_args; i > 0; i--, n <<= 1) {
      p = va_arg (ap, char *);

      if (p == NULL) {
	k = 1;
	continue;
      }
      /* fprintf (dfile, "my_strcat () p: (%s)\n", p); */

      /* The intended effect is that if a string is NULL and
       * the next string is a " " (ie, blank space seperator)
       * then skip the " " so the line doesn't have an extra space.
       */
      if (k) {
	k = 0;
	if (!strcmp (" ", p))
	  continue;
      }

      /* Make sure the attribute will fit into our buffer.
       * If the object ends up overflowing, then set the
       * 'attr_too_big' flag which will turn off canonicalization
       * for the attribute (ie, get object from the attr copy file)
       */
      if (!o->attr_too_big) {
	len = strlen (p);
	if ((j + len + 1) < MAX_ATTR_SIZE) {
	  memcpy ((char *) (buf + j), p, len);
	  j += len;
	}
	else
	  o->attr_too_big = 1;
      }

      if (n & skipfree)
	continue;

      /* fprintf (dfile, "my_strcat ():  free (%s)\n", p); */
      free (p);
  }

 va_end (ap);

 buf[j] = '\0';

 /* fprintf (dfile, "my_strcat () returns (%s)\n", buf); */
 return strdup (buf);
}

void wrap_up (canon_info_t *ci) {
  
  if (ci->fd != NULL) {
    fclose (ci->fd);
    unlink (ci->flushfn);
  }
  
  if (ci->lfd != NULL) {
    fclose (ci->lfd);
    unlink (ci->lflushfn);
  }
  
  if (ci->efd != NULL) {
    fclose (ci->efd);
    unlink (ci->eflushfn);
  }

  ci->fd = ci->lfd = ci->efd = NULL; 
}

/* Given a directory, remove all files from the directory
 * and then remove the directory.
 *
 * Return:
 *
 *   void
 */
void rm_tmpdir (char *dir) {
  char f[256];
  struct dirent *dp;
  DIR *dirp;

  /* open the directory and read and remove all the entries */
  if ((dirp = opendir (dir)) != NULL) {
    while ((dp = readdir (dirp)) != NULL)
      if (strcmp (dp->d_name, ".") &&
	  strcmp (dp->d_name, "..")) {
	strcpy (f, dir);
	strcat (f, "/");
	strcat (f, dp->d_name);
	remove (f);
      }

    closedir(dirp);

    /* remove the directory */
    if (rmdir (dir))
      trace (ERROR, default_trace, 
	     "rm_tmpdir (): could not rm tmpdir (%s)\n", strerror (errno));
  }
  else
    trace (ERROR, default_trace, 
	   "rm_tmpdir (): could not open tempdir to remove files (%s)", 
	   strerror (errno));
}

/* Take a "COOKIE" string and save it to our parse info 
 * data structure.  Each "COOKIE" string is copied verbatim
 * with the exception of possibly adding a line feed.
 * The "COOKIE" info has user email from, msg id, subject
 * and date.
 *
 * Return:
 *   void
 */
void save_cookie_info (parse_info_t *pi, char *s) {
  if (s == NULL)
    return;

  pi->cookies = my_concat (pi->cookies, s, 0);
  /* append a line feed if there isn't one */
  if (*(s + strlen (s) - 1) != '\n')
    pi->cookies = my_concat (pi->cookies, "\n", 0);
}

char *todays_strdate () {
  char strdate[9];

  sprintf (strdate, "%d", todays_date ());

  return strdup (strdate);
}

/*
 * All the regular expressions go here.
 */
void init_regexes () {

  regex_compile (re, (int) RE_DATE, "^[[:digit:]]{8}$");
  regex_compile (re, (int) RE_EMAIL1, "\"[^\"@\\]+\"@");
  regex_compile (re, (int) RE_EMAIL2,
		 "^[^]()<>,;:\\\". \t]+(\\.[^]()<>,;:\\\". \t]+)*@");
  regex_compile (re, (int) RE_EMAIL3, 
		 "@[[:alnum:]]+([.-][[:alnum:]]+)*$");
  regex_compile (re, (int) RE_CRYPT_PW, "^[[:alnum:]./]{13}$");
  regex_compile (re, (int) RE_NAME, "^[[:alpha:]][[:alnum:].'`|-]*$");
  regex_compile (re, (int) RE_APNIC_HDL, "^[A-Z]{2}[[:digit:]]{3}JP(-JP)?$");
  regex_compile (re, (int) RE_LCALPHA, "[a-z]");
  regex_compile (re, (int) RE_STD_HDL, "^[A-Z]{1,4}([1-9][[:digit:]]{0,5})?(-[[:graph:]]+)?(-NIC)?$");
  regex_compile (re, (int) RE_RIPE_HDL, "^AUTO-[[:digit:]]+[A-Z]*$");
  regex_compile (re, (int) RE_REAL, "^[+-]?([[:digit:]]+)?.[[:digit:]]+(E[+-]?[[:digit:]]+)?$");
  regex_compile (re, (int) RE_ASNAME, "^[A-Z][A-Z0-9-]+$");
  /* needed by lc_lex () */
  regex_compile (re, (int) RE_ASNUM, "^AS[1-9][0-9]{0,9}$");
  regex_compile (re, (int) RE_ARIN_HDL, "^[A-Z]{2,4}([1-9][[:digit:]]{0,5})?-ARIN$");
  regex_compile (re, (int) RE_SANITY_HDL, "^[[:alpha:]][[:alnum:]-]{1,}[[:alnum:]]+$");
}

/* Compile the regular expression 'reg_exp' into array 're'.
 *
 * Return:
 *
 *   void
 */
void regex_compile (regex_t re[], int offset, char *reg_exp) {
  
  if (regcomp(&re[offset], reg_exp, REG_EXTENDED|REG_NOSUB) != 0) {
    trace (ERROR, default_trace, "regex_compile(): Couldn't compile regular expression (%s), abort!\n", reg_exp);
    exit (0);
  }      
}
