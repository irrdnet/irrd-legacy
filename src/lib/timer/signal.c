/*
 * $Id: signal.c,v 1.1.1.1 2000/02/29 22:28:54 labovit Exp $
 */

#include <mrt.h>
#include <timer.h>

#if defined(__linux__) && defined(HAVE_LIBPTHREAD)
/* Linux pthread is different from Posix Pthread on asynchronous
   signal handling of SIGALRM that is used to run timers in MRT.
   The signal is delivered to the thread having called alarm().
   The alarm value may be wanted to change from other running threrads, but
   it's not easy to change the alarm value while waiting sigwait() by
   the thread waiting on.  So, with linux pthread, the signal handler 
   just sets a flag as if there is no thread support. All threads
   may be interrupted by the alarm signal. Don't block SIGALRM. */
#endif

void
init_mrt_thread_signals ()
{
#ifdef HAVE_LIBPTHREAD
    sigset_t set;

    sigemptyset (&set);
#ifndef __linux__
    sigaddset (&set, SIGALRM);
#endif /* __linux__ */
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

  /* this may happen during trace(), so don't use trace() again. */
  /* trace (TR_WARN, MRT->trace, "MRT signal (%d) received\n", sig); */
  if (sig == SIGINT) MRT->force_exit_flag = sig;
  /* Should we exit in all cases? */
  signal (sig, mrt_process_signal);
}


/* function where alarm is delivered */
static void_fn_t timer_fire_fn = NULL;

#if !defined(HAVE_LIBPTHREAD) || defined (__linux__)
/* Thread uses sigwait(), instead */

static int alarm_pending = 0;
#ifdef HAVE_LIBPTHREAD
static int thread_id = 0;
#endif /* HAVE_LIBPTHREAD */

/* This is the only function that's invoked by asynchronous signal.
   most functions are not asynchronous signal safe, so don't call
   them during interrupt. Timers MRT uses are second order so that
   they don't require rapid real-time processing */
void 
alarm_interrupt (/* void */)
{
    alarm_pending++;
#ifdef HAVE_LIBPTHREAD
    thread_id = pthread_self ();
#endif
}
#endif /* HAVE_LIBPTHREAD */


/* check to see if alarm is pending */
void
mrt_alarm (void)
{
#if defined(HAVE_LIBPTHREAD) && !defined(__linux__)
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
#ifdef HAVE_LIBPTHREAD
       trace (TR_TIMER, TIMER_MASTER->trace, "ALRM caught by thread %d\n", 
	      thread_id);
#endif
	alarm_pending = 0;
	if (timer_fire_fn)
	    timer_fire_fn ();
    }
#ifdef HAVE_LIBPTHREAD
    /* blocking is OK since this thread is idling */
    pause ();
#endif
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

#if !defined(HAVE_LIBPTHREAD) || defined(__linux__)
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
#if defined(HAVE_LIBPTHREAD) && !defined(__linux__)
    /* sigwait() will be used so that ALARM must be blocked 
       even in the main thread */
    pthread_sigmask (SIG_BLOCK, &set, NULL);
#else
    /* enables alarm interrupts */
    pthread_sigmask (SIG_UNBLOCK, &set, NULL);
#endif /* HAVE_LIBPTHREAD */
    timer_fire_fn = fn;
}
