/*
 * $Id: trace.c,v 1.4 2002/10/17 20:12:02 ljb Exp $
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/types.h>
#include <ctype.h>
#include <time.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <unistd.h>
#include <syslog.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/errno.h>
#include <errno.h>
#include <trace2.h>
#include <irrauth.h>
#include <hdr_comm.h>

#define REOPEN_FILE_AFTER_BYTES	  600
#define BIT_TEST(f, b)  ((f) & (b))

/* internal routines */
static FILE *get_trace_fd (trace_t * trace_struct);
static char *_my_strftime (char *tmp, long in_time, char *fmt);
char *uii_parse_line (char **line);

static int syslog_notify = 0;

int
init_trace (name, syslog_flag)
     const char *name;
     int syslog_flag;
{
    if (syslog_flag) {
	openlog (name, LOG_PID, LOG_DAEMON);
	syslog_notify = 1;
    }
    return (1);
}

/* trace
 */
int
trace (int flag, trace_t * tr,...)
{
    va_list args;
    char *format;
    char *ptime;
    int ret, trace_len, ret2;
    char tmp[MAXLINE], tmp2[MAXLINE];

    if ((tr == NULL) || (tr->logfile && tr->logfile->logfd == NULL)) {
	return (0);
    }

    /* nope, we don't log this */
    if (!BIT_TEST (tr->flags, flag)) 
      return (0);

    va_start (args, tr);
    format = va_arg (args, char *);

    /* Generate the trace output string in tmp for processing */
    ptime = (char *) _my_strftime (tmp2, 0, "%h %e %T");

    sprintf (tmp, "%s [%d] ", ptime, (int) getpid ());
    if (tr->prepend && /* XXX */ !isspace((int) format[0])) {
        if (BIT_TEST (flag, TR_WARN))
	    strcat (tmp, "*WARN* ");
        if (BIT_TEST (flag, TR_ERROR))
	    strcat (tmp, "*ERROR* ");
        if (BIT_TEST (flag, TR_FATAL))
	    strcat (tmp, "*FATAL* ");
        sprintf (tmp + strlen (tmp), "%s ", tr->prepend);
    }
    vsprintf ((tmp + strlen(tmp)), format, args);  /* concatenate */
    trace_len = strlen(tmp);

    if (BIT_TEST (flag, TR_WARN | TR_ERROR | TR_FATAL)) {
        if (syslog_notify)
	    syslog (LOG_INFO, tmp + strlen (ptime) + 1);
    }

    /* nope, we don't log this message */
    if ((!BIT_TEST (tr->flags, flag)) || (tr->logfile->logfd == NULL)) {
	/* unlock if we are not locking it in trace_open */
      if (BIT_TEST (flag, TR_FATAL))
	exit (1);
      return (0);
    }

    /* Check length of trace output file and, if it is too long,
     * truncate it to zero.  Don't try to check or truncate stdout and
     * stderr.  Also, if max_filesize is 0, don't truncate.  */
    if (tr->logfile->max_filesize && (tr->logfile->logfd != stdout) && (tr->logfile->logfd != stderr)) {

	/* Do a running check of the logfile size.  Much better. */
        if ((tr->logfile->logsize + trace_len) >= tr->logfile->max_filesize) {

	    /* reopen file to truncate */
	    tr->logfile->logsize = 0;
	    tr->logfile->bytes_since_open = 0;
	    tr->logfile->logfd = freopen(tr->logfile->logfile_name, "w", tr->logfile->logfd);

	    if (!tr->logfile->logfd) { 	/* oh, oh, try one more time */
		tr->logfile->append_flag = FALSE;
		tr->logfile->logfd = get_trace_fd(tr);
		if (!tr->logfile->logfd) {	/* No more log file! */
		    fprintf(stderr, 
			"IRRd Trace Panic!  Unable to open logfile %s: %s!\n",
			tr->logfile->logfile_name, strerror(errno));
		    return -1;
		}
	    }
	}
    }


    /* we periodically reopen, and append to file after writing xxx number of bytes. 
     * This is  so if don't /dev/null log data if file removed,
     * but we still have inode
     * Pfff -- we have to update all of children's fds!. FIX!!!!!!!
     */
    if ((tr->logfile->bytes_since_open > REOPEN_FILE_AFTER_BYTES) && 
	(tr->logfile->logfd != stdout) && (tr->logfile->logfd != stderr)) {

      tr->logfile->bytes_since_open = 0;

      tr->logfile->logfd = freopen(tr->logfile->logfile_name, "a", tr->logfile->logfd);

      if (!tr->logfile->logfd) { 	/* oh, oh, try one more time */
	tr->logfile->append_flag = FALSE;
	tr->logfile->logfd = get_trace_fd(tr);
	if (!tr->logfile->logfd) {	/* No more log file! */
	  fprintf(stderr, 
		  "IRRd Trace Panic!  Unable to open logfile %s: %s!\n",
		  tr->logfile->logfile_name, strerror(errno));
	  return -1;
	}
      }
    }

    /* Print out the pregenerated string.
     * WARNING:  if this string contains any valid printf formatting 
     * characters, weird things could happen!
     */
#ifdef notdef
    ret = fprintf (tr->logfile->logfd, tmp);
#else
    /*
     * Okay, now I think it's safer and compatible -- masaki
     */
    ret = fputs (tmp, tr->logfile->logfd);
#endif
    if ((ret2 = fflush (tr->logfile->logfd)) != 0) {
      /* oops fflush failed */
      /* close socket ?? */
    }
    tr->logfile->logsize += trace_len;
    tr->logfile->bytes_since_open += trace_len;

    /* check if errors */
    if (ret < 0) {
	syslog (LOG_INFO, "fprintf failed: %s", strerror (errno));
	/*mrt_exit (1);*/
	/* turn off tracing? Or is it okay if we keep on failing?
	* probably not -- we don't want to fill up syslog with messages
	*
	*/
	tr->flags = 0; /* turn off tracing */
	return (-1);
    }
    if (BIT_TEST (flag, TR_FATAL))
	abort(); /* mrt_exit (1); */

    return (1);
}

/* New_Trace2
 * Creates a new trace record and initializes it to default values.
 * The app_name parameter is used as the application name, and a default
 * log is created in /tmp/app_name.log
 */
trace_t *
New_Trace2 (char *app_name)
{
    trace_t *tmp;
    char log_name[256];

    if (!app_name) return NULL;
    sprintf(log_name, "/tmp/%s.log", app_name);

    tmp = calloc (sizeof (trace_t), 1);	/* need to use calloc to zero out */
    tmp->logfile = calloc (sizeof (logfile_t), 1);
    tmp->logfile->ref_count = 1;

    /* Duplicate the logfile name now, so it can safely be freed later
     * if we want to change it.  (See set_trace for where it's freed).
     */
    tmp->logfile->append_flag = 1;
    tmp->logfile->logfile_name = strdup(log_name);
    tmp->logfile->logfd = get_trace_fd (tmp);
    tmp->logfile->max_filesize = TR_DEFAULT_MAX_FILESIZE;
    tmp->flags = TR_DEFAULT_FLAGS;
    tmp->syslog_flag = TR_DEFAULT_SYSLOG;

    return (tmp);
}

/* New_Trace
 * For backwards compatibility.  Calls New_Trace2 with a default application
 * name of mrt.
 */
trace_t *
New_Trace (void)
{
    return New_Trace2("irrd");
}

/*
 * Destroy_Trace
 */
void Destroy_Trace (trace_t * tr) {

  if (--tr->logfile->ref_count <= 0) {
      free (tr->logfile->logfile_name);
      if (tr->logfile->prev_logfile)
        free (tr->logfile->prev_logfile);
      fflush (tr->logfile->logfd); /* I can not close */
      if (tr->logfile->logfd != stdout && tr->logfile->logfd != stderr)
         fclose (tr->logfile->logfd);
      free (tr->logfile);
  }
  if (tr->prepend)
    free (tr->prepend);
  free (tr);
}

/* get_trace_fd
 */
static FILE *get_trace_fd (trace_t * tr) {
    char *type;
    FILE *retval;
    struct stat stats;
    char error[255] = "";

    if (!tr)
	return (stdout);

    if (tr->logfile->logfd && tr->logfile->logfd != stdout && tr->logfile->logfd != stderr) {

	fclose (tr->logfile->logfd);

	/* If the previously opened file wasn't used 
	 * (i.e. size = 0), delete it. */
        if (tr->logfile->prev_logfile) {
 	  stat (tr->logfile->prev_logfile, &stats);
	  if (!stats.st_size) {
	    if (unlink(tr->logfile->prev_logfile) < 0) {
		sprintf(error, "unlink %s:  %s\n", tr->logfile->prev_logfile,
			strerror(errno));
	    }
	  }
	}
    }

    if (!strcasecmp (tr->logfile->logfile_name, "stdout")) {
	if (error[0]) fprintf(stdout, error);
	return (stdout);
    }

    if (!strcasecmp (tr->logfile->logfile_name, "stderr")) {
	if (error[0]) fprintf(stderr, error);
	return (stderr);
    }

    if (tr->logfile->logfile_name) {
	if (tr->logfile->append_flag)
	    type = "a";
	else
	    type = "w";
	if ((retval = fopen (tr->logfile->logfile_name, type))) {

	    tr->logfile->logsize = 0;
	    tr->logfile->bytes_since_open = 0;
	    tr->logfile->max_filesize = TR_DEFAULT_MAX_FILESIZE;

	    if (error[0]) fprintf(retval, error);
	    return (retval);
	} /*else
	  fprintf(stderr, "fopen %s:  %s\n", tr->logfile->logfile_name,
		strerror(errno));*/
    }
    return (NULL);
}

/* my_strftime
 * Given a time long and format, return string. 
 * If time <=0, use current time of day
 */
static
char *
_my_strftime (char *tmp, long in_time, char *fmt)
{
    time_t t;
#if defined(_REENTRANT) && defined(HAVE_LOCALTIME_R)
    struct tm tms;
#endif /* HAVE_LOCALTIME_R */

    if (in_time <= 0)
	t = time (NULL);
    else
	t = in_time;

#if defined(_REENTRANT) && defined(HAVE_LOCALTIME_R)
    localtime_r (&t, &tms);
    strftime (tmp, MAXLINE, fmt, &tms);
#else
    strftime (tmp, MAXLINE, fmt, localtime (&t));
#endif /* HAVE_LOCALTIME_R */

    return (tmp);
}

/* set_trace
 */
int set_trace (trace_t * tmp, int first,...) {
    va_list ap;
    enum Trace_Attr attr;

    if (tmp == NULL)
	return (-1);

    /* Process the Arguments */
    va_start (ap, first);
    for (attr = (enum Trace_Attr) first; attr;
	 attr = va_arg (ap, enum Trace_Attr)) {
	switch (attr) {
	case TRACE_LOGFILE:
	    if (tmp->logfile->logfile_name) {
	      if (tmp->logfile->prev_logfile)
		free (tmp->logfile->prev_logfile);
	      tmp->logfile->prev_logfile = tmp->logfile->logfile_name;
	    }
	    tmp->flags |= NORM;
	    tmp->logfile->append_flag = 1;
	    tmp->logfile->logfile_name = strdup (va_arg (ap, char *));
	    tmp->logfile->logfd = get_trace_fd (tmp);
    	    tmp->logfile->bytes_since_open = 0;
    	    tmp->logfile->logsize = 0;
    	    tmp->logfile->max_filesize = TR_DEFAULT_MAX_FILESIZE;
	    break;
	case TRACE_FLAGS:
	case TRACE_ADD_FLAGS:
	    tmp->flags |= va_arg (ap, long);
	    break;
	case TRACE_DEL_FLAGS:
	    tmp->flags &= ~va_arg (ap, long);
	    break;
	case TRACE_USE_SYSLOG:
	    tmp->syslog_flag = (u_char) va_arg(ap, int);
	    break;
	case TRACE_MAX_FILESIZE:
	    tmp->logfile->max_filesize = va_arg(ap, u_int);
	    break;
	case TRACE_PREPEND_STRING:
	    if (tmp->prepend)
		free (tmp->prepend);
	    tmp->prepend = strdup (va_arg(ap, char*));
	    break;
	default:
	    break;
	}
    }
    va_end (ap);

    return (1);
}

int config_trace_local (trace_t *tr, char *line) {
    char *token = NULL;

    /* get server or submission */
    if ((token = (char *) uii_parse_line (&line)) == NULL)  
      return (-1); 
    if (strcmp (token, "submission") != 0) 
      return (-1);
    
    /* get file-name, max-size, or verbose */
    if ((token = (char *) uii_parse_line (&line)) == NULL) { 
      return (-1); 
    } 

    /*verbose */
    if (!strcmp (token, "verbose")) {
      tr->flags = TR_ALL;
      return (1);
    }

    /* filename */
    if (!strcmp (token, "file-name")) {
      if ((token = (char *) uii_parse_line (&line)) == NULL) 
        return (-1); 
      trace (NORM, tr, "Logging to %s\n", token);
      set_trace (tr, TRACE_LOGFILE, token, NULL);
      trace (NORM, tr, "---\n");
      tr->flags |= NORM; /* change to norm at some point */
      return (1);
    }

    /* get max. trace filesize (OPTIONAL) */ 
    if (!strcmp (token, "file-max-size")) {
      if ((token = (char *) uii_parse_line (&line)) == NULL)
        return (-11);
      set_trace (tr, TRACE_MAX_FILESIZE, atoi(token), NULL);
      return (1);
    }
    return (1);
}

/* scan line for tokens and return pointer to next one */
char *
uii_parse_line (char **line)
{
    char *cp = *line, *start;

    /* skip spaces */
    while (*cp && isspace ((int) *cp))
        cp++;

    start = cp;
    while (!isspace ((int) *cp) && (*cp != '\0') && (*cp != '\n'))
        cp++;

    if ((cp - start) > 0) {
        if (*cp != '\0')
	  *cp++ = '\0';
        *line = cp;
        return (start);
    }
    return (NULL);
}

