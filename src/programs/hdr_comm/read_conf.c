/*
 * $Id: read_conf.c,v 1.20 2002/10/17 20:12:01 ljb Exp $
 */


#include <stdio.h>
#include <string.h>
#include <strings.h> /* bzero */
#include <errno.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/types.h>
#include <regex.h>
#include <sys/stat.h>
#include <netdb.h>
#include <unistd.h>
#include <read_conf.h>
#include <hdr_comm.h>
#include <irr_defs.h>

extern config_info_t ci;
char     *free_mem        (char *);

static int      find_conf_token       (char **, char **);
static void     db_line          (char *, src_t *);
static char     *char_line       (char *, char *);
static int      port_line        (char *, int *);
static source_t *find_src_object (src_t *, char *);
static char     *get_footer_msg  (char *);
static int      string_trim      (char *, char *);
static char     *make_pgpname    (char *);
static int      add_acl          (char *, char *, acl **);
 
/* Control the process of parsing the conf file and intializing
 * the 'ci' struct with relevant values.  We are interested in 
 * the following values:
 *
 * config value name    default
 * -----------------    -------
 * irrd_port            43
 * irrd_host            localhost
 * super_passwd         NULL ie, no super password accepted unless it is configured
 * submission_log_dir   'irr_directory' irrd.conf value if present
 *                      log_dir == NULL means no logging
 * irr_directory        DEF_DBCACHE see irr_defs.h
 * pgp_dir              NULL ie, user's ~/.pgp is assumed
 * db_admin             NULL ie, do not email new maintainer requests
 * 'ci' (sources struc) empty list
 *
 * Function assumes 'ci' has bee set to all 0's.  Command line options
 * override the irrd.conf options so if a value is set then the corresponding
 * irrd.conf value will be ignored.
 *
 * Result:
 *   1 if irrd.conf file was opened successfully
 *   -1 otherwise
 *   intialize the 'ci' struct
 */
int parse_irrd_conf_file (char *config_fname, trace_t *tr) {
  FILE *fin;
  char buf[MAXLINE+1], *p, *q, *s, *irr_dir = NULL;
  char val[MAXLINE+1];
  int len, offset;
  config_info_t temp_ci = {0};
  src_t sstart;
  source_t *obj;
  regmatch_t rm[9];
  regex_t re_accept, re_auth, re_rpstrust;

  /* rpsdist <db> accept <host> */
  char *accept = "^[ \t]*(([[:graph:]]+)[ \t]+)?accept[ \t]+([[:graph:]]+)[ \t]*\n?$";
  
  /* rpsdist_database <db> authoritative <pgppass> */
  char *auth =  "^[ \t]*([[:graph:]]+)[ \t]+authoritative[ \t]+"
    "((\"(.+)\")|([^\" \t\n]+))[ \t]*\n?$";

  /* rpsdist_database <db> trusted <host> <port> */
  char *rpstrust = "^[ \t]*([[:graph:]]+)[ \t]+trusted[ \t]+"
    "([[:graph:]]+)[ \t]+([[:digit:]]+)[ \t]*\n?$";

  /* compile our regex's */
  regcomp (&re_accept,   accept,   REG_EXTENDED|REG_ICASE);
  regcomp (&re_auth,     auth,     REG_EXTENDED|REG_ICASE);
  regcomp (&re_rpstrust, rpstrust, REG_EXTENDED|REG_ICASE);

  /* load the defaults */
  sstart.first = sstart.last = NULL;
  temp_ci.irrd_port = 43;
  temp_ci.irrd_host = strdup ("localhost");
  temp_ci.log_dir = NULL;  /* default is 'irr_directory' value */
  temp_ci.db_dir  = strdup (DEF_DBCACHE);
  temp_ci.pgp_dir = NULL;
  temp_ci.db_admin = NULL; /* email addr to send new maintainer req's */
  temp_ci.footer_msg = NULL; /* footer text */
  if (ci.footer_msg != NULL) {
    if (ci.footer_msg[0] != (char ) '"') {
      strcpy (buf, "\"");
      strcat (buf, ci.footer_msg);
      strcat (buf, "\"");
      s = get_footer_msg (buf);
    }
    else
      s = get_footer_msg (ci.footer_msg);
    free (ci.footer_msg);
    ci.footer_msg = strdup (s);
  }
  
  /* load the default config file name */
  if (config_fname == NULL)
    config_fname = "/etc/irrd.conf";
  
  /* open the config file */
  if ((fin = fopen (config_fname, "r")) == NULL) {
    fprintf (stderr, "Error opening IRRd conf file \"%s\": %s\n",
             config_fname, strerror(errno));
    return -1;
  }
  
  /* pick off the value of relavent fields */
  while (fgets (buf, MAXLINE, fin) != NULL) {
    p = q = buf;
    if (find_conf_token (&p, &q) > 0) {
      len = (int) (q - p);
      if (len > 5 && !strncmp (p, "irr_database", len)) {
	/* authoritative */
	db_line (q, &sstart);
      }
      else if (len > 15 && !strncmp (p, "rpsdist_database", len)) {
	db_line (q, &sstart);
	
	/* rpsdist trusted */
	if (regexec (&re_rpstrust, q, 8, rm, 0) == 0) {
	  /* DB name */
	  *(q + rm[1].rm_eo) = '\0';
	  if ((obj = find_src_object (&sstart, q + rm[1].rm_so)) != NULL) {
	    obj->rpsdist_flag = 1;
	    obj->rpsdist_trusted = 1;
	    
	    /* host */
	    *(q + rm[2].rm_eo) = '\0';
	    obj->rpsdist_host = strdup (q + rm[2].rm_so);
	    
	    /* port */
	    *(q + rm[3].rm_eo) = '\0';
	    obj->rpsdist_port = strdup (q + rm[3].rm_so);
	  }
	}

	/* authoritative and pgppass */
	else if (regexec (&re_auth, q, 9, rm, 0) == 0) {
	  /* DB name */
	  *(q + rm[1].rm_eo) = '\0';
	  if ((obj = find_src_object (&sstart, q + rm[1].rm_so)) != NULL) {
	    obj->rpsdist_flag = 1;
	    obj->rpsdist_auth = 1;
	    offset = 5;
	    if (rm[4].rm_so != -1)
	      offset = 4;
	    *(q + rm[offset].rm_eo) = '\0';
	    strcpy (val, (char *) (q + rm[offset].rm_so));
	    obj->rpsdist_pgppass = strdup (val);
	  }
	}
      }

      if (regexec (&re_accept, q, 4, rm, 0) == 0) {
	/* optional DB name(2), address (3) */
	*(q + rm[3].rm_eo) = '\0';
	if( rm[2].rm_so != -1 ){
	  *(q + rm[2].rm_eo) = '\0';
	  add_acl((q + rm[3].rm_so), (q + rm[2].rm_so), &ci.acls);	    
	}
	else
	  add_acl((q + rm[3].rm_so), NULL, &ci.acls);
      }
      else if (len > 4 && !strncmp (p, "irr_port", len))
	temp_ci.irrd_port = port_line (q, &temp_ci.irrd_port);
      else if (len > 4 && !strncmp (p, "irr_host", len))
	temp_ci.irrd_host = char_line (q, temp_ci.irrd_host);
      else if (len > 3 && !strncmp (p, "override_cryptpw", len))
	temp_ci.super_passwd = char_line (q, temp_ci.super_passwd);
      else if (len > 4 && !strncmp (p, "pgp_dir", len))
	temp_ci.pgp_dir = char_line (q, temp_ci.pgp_dir);
      else if (len > 5 && !strncmp (p, "irr_directory", len))
	irr_dir = char_line (q, irr_dir);
      else if (len > 3 && !strncmp (p, "db_admin", len))
	temp_ci.db_admin = char_line (q, temp_ci.db_admin);
      else if (len > 4  && !strncmp (p, "debug", len)) 
	config_trace_local (tr, q);
      else if (len > 10 && ci.footer_msg == NULL &&
	       !strncmp (p, "response_footer", len))
	temp_ci.footer_msg = myconcat_nospace (temp_ci.footer_msg, get_footer_msg (q));
      else if (len > 10 && ci.notify_header_msg == NULL &&
	       !strncmp (p, "response_notify_header", len))
	temp_ci.notify_header_msg =
	  myconcat_nospace (temp_ci.notify_header_msg, get_footer_msg (q));
      else if (len > 10 && ci.forward_header_msg == NULL &&
	       !strncmp (p, "response_forward_header", len))
	temp_ci.forward_header_msg = 
	  myconcat_nospace (temp_ci.forward_header_msg, get_footer_msg (q));
      else if (len > 5 && !strncmp (p, "irr_database", len)) {

      }
    }
  }

  /* If the user has specified command line options,
   * then use those instead of the irrd.conf options
   */
  if (ci.srcs == NULL)
    ci.srcs = sstart.first;

  if (ci.irrd_port == 0)
    ci.irrd_port = temp_ci.irrd_port;

  if (ci.irrd_host == NULL)
   ci.irrd_host = temp_ci.irrd_host;
  else
    free_mem (temp_ci.irrd_host);

  if (ci.super_passwd == NULL)
    ci.super_passwd = temp_ci.super_passwd;
  else
    free_mem (temp_ci.super_passwd);

  if (ci.footer_msg == NULL) {
    if (temp_ci.footer_msg != NULL) {
        ci.footer_msg = strdup (temp_ci.footer_msg);
  	free_mem (temp_ci.footer_msg);
    }
  } 

  if (ci.notify_header_msg == NULL) {
    if (temp_ci.notify_header_msg != NULL) {
        ci.notify_header_msg = strdup (temp_ci.notify_header_msg);
  	free_mem (temp_ci.notify_header_msg);
    }
  } 

  if (ci.forward_header_msg == NULL) {
    if (temp_ci.forward_header_msg != NULL) {
        ci.forward_header_msg = strdup (temp_ci.forward_header_msg);
  	free_mem (temp_ci.forward_header_msg);
    }
  } 

  /* need the db cache directory for rps_dist communication */
  if (irr_dir != NULL)
    ci.db_dir = strdup (irr_dir);

  /* see if the cache dir exists.  we do not need
   * it except for default ack and trans logs
   * and pgp dir */
  if ((s = dir_chks (ci.db_dir, 0)) != NULL) {
    ci.db_dir = NULL;
    trace (NORM, tr, "irr_dir set to NULL:%s\n", s);
    free (s);
  }
    
  /* pgp directory processing, used by RPS-DIST */
  if (ci.pgp_dir == NULL) {
    if (temp_ci.pgp_dir == NULL) {
      ci.pgp_dir = make_pgpname (irr_dir);
    }
    else
      ci.pgp_dir = temp_ci.pgp_dir;
  }
  else
    free_mem (temp_ci.pgp_dir);

  /* see if the pgp dir exists. */
  if ((s = dir_chks (ci.pgp_dir, 0)) != NULL) {
    ci.pgp_dir = NULL;
    trace (NORM, tr, "pgp_dir set to NULL:%s\n", s);
    free (s);
  }

  /* what the hey, we only leak a little memory */
  if (ci.log_dir == NULL) {
    if (temp_ci.log_dir == NULL)
      ci.log_dir = irr_dir;
    else
      ci.log_dir = temp_ci.log_dir;
  }
  else {
    free_mem (irr_dir);
    free_mem (temp_ci.log_dir);
  }

  /* see if the log dir exists. */
  if ((s = dir_chks (ci.log_dir, 0)) != NULL) {
    ci.log_dir = NULL;
    trace (NORM, tr, "log_dir set to NULL:%s\n", s);
    free (s);
  }

  if (ci.db_admin == NULL)
    ci.db_admin = temp_ci.db_admin;
  else
    free_mem (temp_ci.db_admin);

  fclose  (fin);
  regfree (&re_accept);
  regfree (&re_auth);
  regfree (&re_rpstrust);

  return 1;
}


/* Given an irrd.conf line starting with 'irr_port', return
 * the port value.
 *
 * Return:
 *   the port value
 *   echo back *port if there is a missing token after
 *   'irr_port'
 */
int port_line (char *line, int *port) {
  char *p, *q;

  p = q = line;
  if (find_conf_token (&p, &q) > 0) {
    *q = '\0';
    return atoi (p);
  }

  return *port;
}

/* Given an irrd.conf line starting with 'irr_host', return
 * the host value.
 *
 * Routine should be invoked as 'host = host_line (line, host);'
 * as 'host' is freed.  
 *
 * Routine was originally written for 'irrd_host' but can be used for
 * any char * valued conf datum.
 *
 * Return:
 *   char *host value for the irrd.conf file if there are no errors
 *   echo back input 'host' value if there is a missing token
 */
char *char_line (char *line, char *host) {
  char *p, *q;

  p = q = line;
  if (find_conf_token (&p, &q) > 0) {
    *q = '\0';
    free_mem (host);
    return strdup (p);
  }
  else
    return host;
}


/* Given an irrd.conf line of 'irr_database' create a list
 * entry with the DB name and whether the DB is authoritative.
 *
 * Return:
 *   void
 *   a list element appended to list pointed to by 'start' with 
 *   the DB name and authoritative field initialized
 */
void db_line (char *line, src_t *start) {
  char *p, *q, *r, *source, c;
  int len, auth;
  source_t *obj;

  /* first find the DB name */
  p = q = line;
  if (find_conf_token (&p, &q) > 0) {
    auth = 0;
    source = p;
    r = q;

    /* now find the keyword following the DB name */
    if (find_conf_token (&p, &q) > 0) {
      len = (int) (q - p);
      if (len > 1 && !strncmp (p, "authoritative", len))
	auth = 1;
    }

    c = *r;
    *r = '\0';
    if ((obj = find_src_object (start, source)) != NULL) {
      if (auth)
	obj->authoritative = auth;
    }
    else {
      obj = create_source_obj (source, auth);
      add_src_obj (start, obj);
    }
    *r = c;
  }

  return;
}

/* Find the source 'source' in the source list pointed
 * to by 'start'.
 *
 * Return:
 *   pointer to element if 'source' is found
 *   NULL otherwise
 */
source_t *find_src_object (src_t *start, char *source) {
  source_t *obj;

  obj = start->first;
  while (obj != NULL) {
    if (!strcasecmp (obj->source, source))
      break;
    obj = obj->next;
  }

  return obj;
}

/* Create a source_t list element with value 'source' and 
 * 'authoritative'.
 *
 * Return:
 *   void
 */
source_t *create_source_obj (char *source, int authoritative) {
  source_t *obj;
  char *z;

  if (source == NULL)
    return NULL;

  obj = (source_t *) malloc (sizeof (source_t));
  memset ((char *) obj, 0, sizeof (source_t));
  obj->source          = strdup (source);
  obj->authoritative   = authoritative;
  for (z = obj->source; z != NULL && *z != '\0'; *z = toupper ((int) *z), z++);

  return (obj);
}

/* Add a source_t element to the list pointed to by 'start'.
 *
 * Return:
 *   void
 */
void add_src_obj (src_t *start, source_t *obj) {

  obj->next = NULL;
  if (start->first == NULL)
    start->first = obj;
  else
    start->last->next = obj;

  start->last = obj;
}

/* This routine finds a token in the string.  *x will
 * point to the first character in the string and *y will
 * point to the first character after the token.  A token
 * is a printable character string.  A '\n' is not considered
 * part of a legal token.  
 *    It will look for !'s in
 * the string and assume everything after is a comment.
 * Invoke this routine by setting 'x' and 'y' to the beginning
 * of the target string like this: 'x = y = target_string;
 * if (find_conf_token (&x, &y) > 0) ...
 * each successive call to find_conf_token () will move the char 
 * pointers along.
 *
 * Return:
 *   1 if a token is found (*x points to token, *y first char after)
 *  -1 if no token is found (*x and *y are to be ignored)
 */
int find_conf_token (char **x, char **y) {

  /* It's possible the target string is NULL 
   * or we are in an rpsl comment 
   */
  if (*y == NULL || **y == '!')
    return -1;

  *x = *y;
  /* find the start of a token, ie, first printable character */
  while (**x == ' ' || **x == '\t') (*x)++;

  if (**x == '\0' || **x == '!' || **x == '\n')
    return -1;

  /* find the first char at the end of the token */
  *y = *x + 1;
  while (**y != '\0' && (isgraph ((int) **y) && **y != '!')) (*y)++;

  return 1;  
}

/* Return a footer message line.  The line should
 * be enclosed in quotes, eg, "Welcome to IRRd.".
 * If quotes are not supplied, return the string
 * starting with the first non-blank character.
 *
 * Return:
 *   String enclosed in quotes minus the quotes if in form "..."
 *   String verbatim if quotes are missing
 *   NULL otherwise
 */
char *get_footer_msg (char *s) {
  char *q, *r;
  int i  = 0;
  char buf[1024];

  if (s == NULL)
    return NULL;

  /* Find the first printable character */
  for (; isspace ((int) *s); s++);

  /* we have a "..." enclosed string */
  if ((q = strchr (s, '"')) != NULL &&
       (r = strrchr (++q, '"')) != NULL) {
    for (; q < r && i < 1022; q++, i++) {
      if (*q == '\\' && *(q + 1) == 'n') {
	memcpy (&buf[i], "\n", 1);
	q++;
      }
      else
	memcpy (&buf[i], q, 1);
    }

    if (i == 0)
      return NULL;
    else {
      if (buf[i - 1] != '\n')
	memcpy (&buf[i++], "\n", 1);
      buf[i] = '\0';
      return strdup (buf);
    }
  }
  else {
    /* Return whatever the user has typed in 
     * starting with the first non-blank character
     */
    if (*s == '\0')
      return NULL;
    else {
      if (*(s + strlen (s) - 1) != '\n') {
	strcpy (buf, s);
	strcat (buf, "\n");
	return strdup (buf);
      }
      return strdup (s);
    }
  }
}

/* return string 's' with spaces removed in 't' and
 * the length of 't'.
 */
int string_trim (char *s, char *t) {
  int n;
  regmatch_t rm[2];
  regex_t re;
  char *str_trim  = "^[ \t]*(([^ \t])|([^ \t].*[^ \t]))[ \t]*$";

  regcomp (&re, str_trim, REG_EXTENDED);

  if (regexec (&re, s, 2, rm, 0) == 0) {
    /* directory is longer than our buffer */
    if ((n = (rm[1].rm_eo - rm[1].rm_so)) > (MAXLINE - 1))
      return 0;

    strncpy (t, (char *) (s + rm[1].rm_so), n);
    t[n] = '\0';

    return n;
  }

  return 0;
} 

char *make_pgpname (char *dir) {
  int n;
  char pgpname[MAXLINE];

  if (dir == NULL)
    return NULL;

  if (!(n = string_trim (dir, pgpname)))
    return NULL;

  if (n > (MAXLINE - 7))
    return NULL;

  strcat (pgpname, "/.pgp/");	

  return strdup (pgpname);
}



/* Perform basic dir tests and return a textual
 * description of anything we find wrong such as
 * insufficient permissions.
 *
 * Results will tell if (dir) exists and if we have 
 * read/write permissions.  Function will try to
 * create (dir) if it doesn't exist and the
 * (creat_dir) flag is set.
 *
 * Input:
 *  -name of the dir to check (dir)
 *  -flag to indicate if (dir) should be created if it 
 *   doesn't exist
 *
 * Return:
 *  -a text string describing some problem
 *  -NULL otherwise
 */
char *dir_chks (char *dir, int creat_dir) {
  char file[MAXLINE+1];
  FILE *fp;

  /* Sanity checks */
  if (dir == NULL)
    return strdup ("No directory specified!");

  if (strlen (dir) > MAXLINE)
    return strdup ("Directory name is too long! MAX chars (MAXLINE)");
  
  /* see if we can create a temp file in the directory */
  snprintf (file, MAXLINE, "%s/cache-test.%d", dir, (int) getpid ());
  if ((fp = fopen (file, "a")) == NULL) {

    /* dir does not exist, try to make it */
    if (creat_dir &&
	errno == ENOENT) {
      if (!mkdir (dir, 00755))
	return NULL;
    }

    /* we have permission problems or ... */
    snprintf (file, MAXLINE, 
	      "Could not create the directory: %s", strerror (errno));
    return strdup (file);
  } else {
    fclose (fp);
    remove (file);
  }

  return NULL;
}

/* host can either be the dot notation or name of the
 * party you wish to allow
 */
static int add_acl(char * host, char * db_name, acl ** acl_list){
  acl * tmp = *acl_list;
  
  /* find end */
  while(tmp && tmp->next) tmp=tmp->next;
  
  /* insert */
  if(!tmp){
    if( (tmp = (acl *)malloc(sizeof(acl))) == NULL){
      return 0;
    }
    *acl_list = tmp;
  }
  else{
    if( (tmp->next = (acl *)malloc(sizeof(acl))) == NULL){
      return 0;
    }
    tmp = tmp->next;
  }
  
  /* set data */
  bzero(tmp, sizeof(acl));
  tmp->host = strdup(host);
  tmp->db_name = db_name;

  return 1;
}

