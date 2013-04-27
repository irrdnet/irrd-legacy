/*
 * $Id: trace.c,v 1.9 2002/02/04 20:33:03 ljb Exp $
 */

#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#include <mrt.h>
#define _VARARGS_H /* for solaris */
#include <syslog.h>
#include <sys/stat.h>
#include "timer.h"
#include <irrdmem.h>

#define REOPEN_FILE_AFTER_BYTES	  600

/* internal routines */
static FILE *get_trace_fd (trace_t * trace_struct);
static char *_my_strftime (char *tmp, long in_time, char *fmt);

static int syslog_notify = 0;
static void init_struct ();

int
init_trace (name, syslog_flag)
     const char *name;
     int syslog_flag;
{
    if (syslog_flag) {
	openlog (name, LOG_PID, LOG_DAEMON);
	syslog_notify = 1;
    }
    init_struct ();
    return (1);
}

static error_list_t *
new_error_list (int max_errors)
{
    error_list_t *error_list;

    error_list = calloc(sizeof(error_list_t),1);
    /* I want to use Delete in place of free, but it's a macro. */
    error_list->ll_errors = LL_Create (LL_DestroyFunction, free, 0);
    error_list->max_errors = max_errors;
    pthread_mutex_init (&error_list->mutex_lock, NULL);
    return (error_list);
}

static void
delete_error_list (error_list_t *error_list)
{
    LL_Destroy (error_list->ll_errors);
    free(error_list);
}

static void 
add_error_list (error_list_t *error_list, char *message)
{
    pthread_mutex_lock (&error_list->mutex_lock);
    if (LL_GetCount (error_list->ll_errors) > error_list->max_errors) {
	char *s = LL_GetHead (error_list->ll_errors);
	LL_Remove (error_list->ll_errors, s);
	/* free(s); */
    }
    LL_Append (error_list->ll_errors, strdup (message));
    pthread_mutex_unlock (&error_list->mutex_lock);
}

/* trace
 */
int
trace (int flag, trace_t * tr, ...)
{
    va_list args;
    char *format;
    int ret, trace_len, ret2;
    char ptime[BUFSIZE];

    if ((tr == NULL) || (tr->logfile && tr->logfile->logfd == NULL)) {
	return (0);
    }

    /* nope, we don't log this */
    if (!BIT_TEST (tr->flags, flag)) 
      return (0);

    /* check that trace is not open -- blocks until it can get lock */
    if (tr->logfile->thread_id != pthread_self ()) {
#ifdef notdef
	/* for debug */
	if (pthread_mutex_trylock (&tr->logfile->mutex_lock) != 0)
	    abort();
#else
	pthread_mutex_lock (&tr->logfile->mutex_lock);
#endif
    }

    va_start (args, tr);
    format = va_arg (args, char *);

    /* Generate the trace output string in buffer for processing */
    if (tr->buffer == NULL)
        tr->buffer = New_Buffer (0);
    else
	buffer_reset (tr->buffer);

    _my_strftime (ptime, 0, "%h %e %T");
    buffer_printf (tr->buffer, "%s [%u] ", ptime, pthread_self ());
    if (tr->prepend && /* XXX */ !isspace((int)format[0])) {
        if (BIT_TEST (flag, TR_WARN))
	    buffer_printf (tr->buffer, "*WARN* ");
        if (BIT_TEST (flag, TR_ERROR))
	    buffer_printf (tr->buffer, "*ERROR* ");
        if (BIT_TEST (flag, TR_FATAL))
	    buffer_printf (tr->buffer, "*FATAL* ");
        buffer_printf (tr->buffer, "%s ", tr->prepend);
    }

    buffer_vprintf (tr->buffer, format, args);
    trace_len = buffer_data_len (tr->buffer);

    if (BIT_TEST (flag, TR_WARN | TR_ERROR | TR_FATAL)) {
        if (syslog_notify)
	    syslog (LOG_INFO, buffer_data (tr->buffer) + strlen (ptime) + 1);
	if (tr->error_list)
	    add_error_list (tr->error_list, buffer_data (tr->buffer));
    }

    /* wer are logging to a terminal ! */
    /* everything goes to the terminal ! */
    if (MRT->trace->uii != NULL) {
	if (MRT->trace->uii_fn (MRT->trace->uii, tr->buffer) < 0)
	    MRT->trace->uii = NULL;
	    /* disable on write error */
      
#ifdef notdef
      /*
       * I'd like to see a message on both terminal and log -- masaki
       */
      if (tr->logfile->thread_id != pthread_self ())
	pthread_mutex_unlock (&tr->logfile->mutex_lock);

      return (1);
#endif
    }

    /* nope, we don't log this message */
    if ((!BIT_TEST (tr->flags, flag)) || (tr->logfile->logfd == NULL)) {
	/* unlock if we are not locking it in trace_open */
	if (tr->logfile->thread_id != pthread_self ())
	    pthread_mutex_unlock (&tr->logfile->mutex_lock);
	if (BIT_TEST (flag, TR_FATAL))
	    mrt_exit (1);
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
			"MRT Trace Panic!  Unable to open logfile %s: %s!\n",
			tr->logfile->logfile_name, strerror(errno));
		    return -1;
		}
	    }
	}
    }

    /* Print out the pregenerated string. */
    ret = fputs (buffer_data (tr->buffer), tr->logfile->logfd);

    if ((ret2 = fflush (tr->logfile->logfd)) != 0) { 
      /* oops fflush failed */
      /* close socket ?? */
    }
    tr->logfile->logsize += trace_len;
    tr->logfile->bytes_since_open += trace_len;

    /* unlock if we are not locking this in trace_open */
    if (tr->logfile->thread_id != pthread_self ())
	pthread_mutex_unlock (&tr->logfile->mutex_lock);

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

    tmp = calloc(sizeof(trace_t),1);
    tmp->logfile = calloc(sizeof(logfile_t),1);
    tmp->logfile->ref_count = 1;

    /* Duplicate the logfile name now, so it can safely be freed later
     * if we want to change it.  (See set_trace for where it's freed).
     */
    tmp->logfile->logfile_name = strdup(log_name);
    tmp->logfile->prev_logfile = strdup("");
    tmp->logfile->logsize = 0;
    tmp->logfile->bytes_since_open = 0;
    tmp->logfile->max_filesize = TR_DEFAULT_MAX_FILESIZE;
    tmp->logfile->append_flag = TRUE;
    tmp->flags = TR_DEFAULT_FLAGS;
    tmp->syslog_flag = TR_DEFAULT_SYSLOG;
    tmp->logfile->logfd = get_trace_fd (tmp);
    if (!tmp->logfile->logfd) {
      fprintf(stderr, "Trace Panic!  Unable to open logfile %s: %s!\n",
		  tmp->logfile->logfile_name, strerror(errno));
	  return (NULL);
    }
    tmp->uii = NULL;
    tmp->prepend = NULL;
    tmp->error_list = NULL;

    tmp->logfile->thread_id = (pthread_t) -1;
    pthread_mutex_init (&tmp->logfile->mutex_lock, NULL);

    return (tmp);
}

/* New_Trace
 * For backwards compatibility.  Calls New_Trace2 with a default application
 * name of mrt.
 */
trace_t *
New_Trace (void)
{
    return New_Trace2("mrt");
}

/* trace_copy
 *
 * JMH - the primary thing that distinguishes a master trace and a slave
 * trace is the logfile datastructure.  A slave can have all the other
 * attributes changed, however the logfile will stay the same.
 * The moment we use a new logfile structure, we're no longer a slave.
 *
 * This is an important distinction since some code seems to use
 * trace_copy and an immediate call to set_trace because we have no
 * overloaded New function with a copy constructor.
 *
 */
trace_t *
trace_copy (trace_t * old)
{
    trace_t *tmp = NULL;

    if (old == NULL)
	return (NULL);

    tmp = calloc(sizeof(trace_t),1);
    tmp->slave = 1;	  /* we are a copy of the "master" who owns fd */
    /* I'm not sure "slave" is still needed */
    tmp->logfile = old->logfile;
    tmp->logfile->ref_count++;
    tmp->flags = old->flags;
    if (old->prepend)
        tmp->prepend = strdup (old->prepend);
    if (old->error_list)
        tmp->error_list = new_error_list (old->error_list->max_errors);

    /* add to global list */
    pthread_mutex_lock (&MRT->mutex_lock);
    LL_Add (MRT->ll_trace, tmp);
    pthread_mutex_unlock (&MRT->mutex_lock);

    return (tmp);
}

/*
 * Destroy_Trace
 */
void Destroy_Trace (trace_t * tr) {

  /* remove from global list */
  pthread_mutex_lock (&MRT->mutex_lock);
  LL_Remove (MRT->ll_trace, tr);
  pthread_mutex_unlock (&MRT->mutex_lock);

  assert (tr->logfile->ref_count > 0);
  if (--tr->logfile->ref_count <= 0) {
      pthread_mutex_destroy (&tr->logfile->mutex_lock);
      free(tr->logfile->logfile_name);
      free(tr->logfile->prev_logfile);
      fflush (tr->logfile->logfd); /* I can not close */
      if (tr->logfile->logfd != stdout && tr->logfile->logfd != stderr)
         fclose (tr->logfile->logfd);
      free(tr->logfile);
  }
  if (tr->prepend)
      free(tr->prepend);
  if (tr->error_list)
      delete_error_list (tr->error_list);
  if (tr->buffer)
     Delete_Buffer (tr->buffer);
  free(tr);
}

/* get_trace_fd
 */
static FILE *get_trace_fd (trace_t * tr) {
    char *type;
    struct stat stats;
    char error[255] = "";

    if (!tr)
	return (stdout);

    if (tr->logfile->logfd && (tr->logfile->logfd != stdout)) {

	fclose (tr->logfile->logfd);

	/* If the previously opened file wasn't used 
	 * (i.e. size = 0), delete it. */
	stat (tr->logfile->prev_logfile, &stats);

	if (!stats.st_size) {
	    if (unlink(tr->logfile->prev_logfile) < 0) {
		sprintf(error, "unlink %s:  %s\n", tr->logfile->prev_logfile,
			strerror(errno));
	    }
	}
    }

    if (!strcasecmp (tr->logfile->logfile_name, "stdout")) {
	tr->logfile->logfd = (FILE *) stdout;
	if (error[0]) fprintf(tr->logfile->logfd, error);
	return (tr->logfile->logfd);
    }

    if (tr->logfile->logfile_name) {
	if (tr->logfile->append_flag)
	    type = "a";
	else
	    type = "w";
	if ((tr->logfile->logfd = fopen (tr->logfile->logfile_name, type))) {

          tr->logfile->logsize = ftell(tr->logfile->logfd);
          tr->logfile->bytes_since_open = 0;
          tr->logfile->max_filesize = TR_DEFAULT_MAX_FILESIZE;

	  if (error[0]) fprintf(tr->logfile->logfd, error);
	  return (tr->logfile->logfd);
	} /*else
	  fprintf(stderr, "fopen %s:  %s\n", tr->logfile->logfile_name,
		strerror(errno));*/

    }
    tr->logfile->logfd = NULL;
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
    strftime (tmp, BUFSIZE, fmt, &tms);
#else
    strftime (tmp, BUFSIZE, fmt, localtime (&t));
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
    	    pthread_mutex_lock (&tmp->logfile->mutex_lock);
 	    if (tmp->slave) {
	      /* JMH - Since we're not a slave, decrement the master refcount */
	      tmp->logfile->ref_count--;
    	      pthread_mutex_unlock (&tmp->logfile->mutex_lock);
	      tmp->slave = 0; /* slave no more */
	      /* we should really make our own mutex lock here */
	      tmp->logfile = calloc(sizeof(logfile_t),1);
	      tmp->logfile->ref_count = 1;
    	      tmp->logfile->prev_logfile = strdup ("");
    	      pthread_mutex_init (&tmp->logfile->mutex_lock, NULL);
    	      pthread_mutex_lock (&tmp->logfile->mutex_lock);
	    }
	    else {
	        if (tmp->logfile->logfile_name) {
	            if (tmp->logfile->prev_logfile) 
			free(tmp->logfile->prev_logfile);
		    tmp->logfile->prev_logfile = tmp->logfile->logfile_name;
	        }
	    }
	    tmp->logfile->logfile_name = strdup (va_arg (ap, char *));
	    tmp->logfile->logfd = get_trace_fd (tmp);
    	    pthread_mutex_unlock (&tmp->logfile->mutex_lock);
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
   	    if (tmp->slave) break; /* ignore */
    	    pthread_mutex_lock (&tmp->logfile->mutex_lock);
	    tmp->logfile->max_filesize = va_arg(ap, u_int);
    	    pthread_mutex_unlock (&tmp->logfile->mutex_lock);
	    break;
	case TRACE_PREPEND_STRING:
	    if (tmp->prepend)
		free(tmp->prepend);
	    tmp->prepend = strdup (va_arg(ap, char*));
	    break;
	case TRACE_MAX_ERRORS:
    	    if (tmp->error_list == NULL)
        	tmp->error_list = new_error_list (va_arg(ap, int));
	    else
		tmp->error_list->max_errors = va_arg(ap, int);
	    break;
	default:
	    break;
	}
    }
    va_end (ap);

    if (!tmp->slave) set_trace_global (tmp);
    return (1);
}

/* trace_generate_file_nam
 * what is this used for?
 */
char *
trace_generate_file_name (char *name)
{
    char *tmp;
    int size;

    size = strlen (name) + 10;
    tmp = malloc(sizeof(char) * size);

    sprintf (tmp, "%s.%d", name, (int) getpid ());

    return (tmp);
}

/* trace_open
 * open a trace for multi-line traces. Obtains
 * mutex lock and marks trace structure with 
 * thread_id
 */
int
trace_open (trace_t * tr)
{

    if (tr == NULL)
	return (-1);


    pthread_mutex_lock (&tr->logfile->mutex_lock);
    tr->logfile->thread_id = pthread_self ();


    return (1);
}

/* trace_close
 */
int
trace_close (trace_t * tr)
{

    if (tr == NULL) {
	return (-1);
    }

    if (pthread_self () != tr->logfile->thread_id) {
		printf ("REALLY BOGUS -- my ID did not open this trace\n");
		exit (0);
    }

    tr->logfile->thread_id = (pthread_t) -1;
    pthread_mutex_unlock (&tr->logfile->mutex_lock);

    return (1);
}

/* trace_flag
 * convert ascii description of trace flag to flag bit
 */
u_int 
trace_flag (char *str)
{

    if (!strcasecmp (str, "info"))
	return (TR_INFO);
    if (!strcasecmp (str, "norm"))
	return (TR_INFO);
    if (!strcasecmp (str, "trace"))
	return (TR_TRACE);
    if (!strcasecmp (str, "parse"))
	return (TR_PARSE);
    if (!strcasecmp (str, "packet"))
	return (TR_PACKET);
    if (!strcasecmp (str, "state"))
	return (TR_STATE);
    if (!strcasecmp (str, "timer"))
	return (TR_TIMER);
    if (!strcasecmp (str, "all"))
	return (TR_ALL);

    return (0);
}

#ifndef HAVE_STRERROR
char *
strerror (int errnum)
{
    extern int sys_nerr;
    extern char *sys_errlist[];
    char *tmpx;

    if (errnum >= 0 && errnum < sys_nerr) {
        return sys_errlist[errnum];
    }

    THREAD_SPECIFIC_STORAGE (tmpx);
    sprintf(tmpx, "Unknown error: %d", errnum);
    return (tmpx);
}
#endif /* HAVE_STRERROR */

static void
init_struct ()
{
}

/* 
 * set_trace globally
 *
 * JMH - This function seems to be misnamed.  What we do here is set
 * the slave traces to contain the same flags and syslog_flags as
 * the master that you pass in.
 *
 * We exploit the fact that a slave trace has the same pointer to
 * the logfile datastructure to make sure that its a slave of the
 * master that we pass in..
 * 
 */
int set_trace_global (trace_t *tmp) {
  trace_t *p_trace;

  assert (tmp);
  
  if (tmp->slave != 0) return (0);

  if ((MRT == NULL) || (MRT->ll_trace == NULL)) return (0);

  LL_Iterate (MRT->ll_trace, p_trace) {
    if ((p_trace->slave == 1) && (p_trace->logfile == tmp->logfile)) {
      p_trace->flags = tmp->flags;
      p_trace->syslog_flag = tmp->syslog_flag;
    }
  }
  return (1);
}

int okay_trace (trace_t *tr, int flags) {

  if (tr && BIT_TEST (tr->flags, flags))
    return (1);

  return (0);
}
