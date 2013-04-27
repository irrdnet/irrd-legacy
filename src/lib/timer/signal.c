/*
 * $Id: signal.c,v 1.1.1.1 2000/02/29 22:28:54 labovit Exp $
 */
#include <netinet/in.h>
#include <arpa/inet.h>

#include <mrt.h>
#include <timer.h>
#include "timer.h"

static char *_my_strftime (char *tmp, long in_time, char *fmt);

void
init_mrt_thread_signals ()
{
#ifdef HAVE_LIBPTHREAD
    sigset_t set;

    sigemptyset (&set);
    sigaddset (&set, SIGHUP);
    sigaddset (&set, SIGINT);
    sigaddset (&set, SIGPIPE);
    pthread_sigmask (SIG_BLOCK, &set, NULL);
#endif /* HAVE_LIBPTHREAD */
}


/* mrt_process_signal
 * invoke registered handler routines, and exit on sigint
 */
void mrt_process_signal (int sig) {
  char *type;
  char ptime[32];
  char message_buf[128];

  /* this may happen during trace(), so don't use trace() again. */
  if (sig == SIGINT) MRT->force_exit_flag = sig;
  if (sig == SIGHUP)  {
    /* check that trace is not open -- blocks until it can get lock */
    if (MRT->trace->logfile->thread_id != pthread_self ()) {
      pthread_mutex_lock (&MRT->trace->logfile->mutex_lock);
    }
    if (MRT->trace->logfile->logfd != stdout && MRT->trace->logfile->logfd != stderr) {
    _my_strftime (ptime, 0, "%h %e %T");
    sprintf (message_buf, "%s [%lu] IRRD HUP received - Reopening default trace file\n", ptime, (unsigned long) pthread_self ());
    fputs (message_buf, MRT->trace->logfile->logfd);
    fclose (MRT->trace->logfile->logfd);
    if (MRT->trace->logfile->append_flag)
      type = "a";
    else
      type = "w";
    if ((MRT->trace->logfile->logfd = fopen (MRT->trace->logfile->logfile_name, type))) {

          MRT->trace->logfile->logsize = ftell(MRT->trace->logfile->logfd);
          MRT->trace->logfile->bytes_since_open = 0;

     } /*else
          fprintf(stderr, "fopen %s:  %s\n", tr->logfile->logfile_name,
                strerror(errno));*/
    }
    if (MRT->trace->logfile->thread_id != pthread_self ()) {
      pthread_mutex_unlock (&MRT->trace->logfile->mutex_lock);
    }
  }
  /* Should we exit in all cases? */
  signal (sig, mrt_process_signal);
}


/* function where alarm is delivered */
static void_fn_t timer_fire_fn = NULL;

#if !defined(HAVE_LIBPTHREAD)
/* Thread uses sigwait(), instead */

static int alarm_pending = 0;

/* This is the only function that's invoked by asynchronous signal.
   most functions are not asynchronous signal safe, so don't call
   them during interrupt. Timers MRT uses are second order so that
   they don't require rapid real-time processing */
void 
alarm_interrupt (/* void */)
{
    alarm_pending++;
}
#endif /* HAVE_LIBPTHREAD */


/* check to see if alarm is pending */
void
mrt_alarm (void)
{
#if defined(HAVE_LIBPTHREAD)
    int sig;
    sigset_t set;

    sigemptyset (&set);
    /* other threads block these signals. taken care of here */
    sigaddset (&set, SIGALRM);
    sigaddset (&set, SIGHUP);
    sigaddset (&set, SIGINT);
    sigaddset (&set, SIGPIPE);

    /* wait on alarm */
    if (sigwait (&set, &sig) < 0) {
       trace (TR_TIMER, TIMER_MASTER->trace, "sigwait: %m\n");
    }
    else {
	if (sig == SIGALRM) {
	    if (timer_fire_fn)
	        timer_fire_fn ();
	}
	else if (sig >= 0)
	    mrt_process_signal (sig);
    }
#else
    if (alarm_pending) {
        alarm_pending = 0;
        if (timer_fire_fn)
            timer_fire_fn ();
    }

#endif /* HAVE_LIBPTHREAD */
}

#ifdef notdef
sigset_t
block_signal (int sig)
{
    sigset_t new, old;

    sigemptyset (&new);
    sigaddset (&new, sig);
#ifdef HAVE_LIBPTHREAD
    pthread_sigmask (SIG_BLOCK, &new, &old);
#else
    sigprocmask (SIG_BLOCK, &new, &old);
#endif /* HAVE_LIBPTHREAD */
    return (old);
}


void
recover_signal (sigset_t old)
{
#ifdef HAVE_LIBPTHREAD
    pthread_sigmask (SIG_SETMASK, &old, NULL);
#else
    sigprocmask (SIG_SETMASK, &old, NULL);
#endif /* HAVE_LIBPTHREAD */
}
#endif


void
init_signal (trace_t *tr, void_fn_t fn)
{
    sigset_t set;

#if !defined(HAVE_LIBPTHREAD)
#ifdef HAVE_SIGACTION /* POSIX actually */
    {
    	struct sigaction act;

        memset (&act, 0, sizeof (act));
        act.sa_handler = alarm_interrupt;
#ifdef SA_RESTART
        act.sa_flags = SA_RESTART; 
#endif
        sigaction (SIGALRM, &act, NULL);
    }
#else
#ifdef HAVE_SIGSET
    sigset (SIGALRM, alarm_interrupt);
#else
    signal (SIGALRM, alarm_interrupt);
#endif /* HAVE_SIGSET */
#endif /* POSIX signals */
#endif /* HAVE_LIBPTHREAD */

   /* This init routine must be called from the main thread
      that handles alarm interrupts before creations of threads */
    sigemptyset (&set);
    sigaddset (&set, SIGALRM);
#if defined(HAVE_LIBPTHREAD)
    /* sigwait() will be used so that ALARM must be blocked 
       even in the main thread */
    pthread_sigmask (SIG_BLOCK, &set, NULL);
#else
    /* enables alarm interrupts */
    pthread_sigmask (SIG_UNBLOCK, &set, NULL);
#endif /* HAVE_LIBPTHREAD */
    timer_fire_fn = fn;
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

