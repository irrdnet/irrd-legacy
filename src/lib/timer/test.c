/*
 * $Id: test.c,v 1.1.1.1 2000/02/29 22:28:54 labovit Exp $
 */

#include <stdarg.h>
#include <mrt.h>
#include <timer.h>


#ifdef HAVE_LIBPTHREAD
/* alarm is critical to mrtd but it works differently with pthreads.
   this program aims to test how different it is on this platform */

static int waiting_time = 5;
static int volatile fired = 0;
static sigset_t mysigset;

static void
select_wait (int sec)
{
    fd_set fdset;
    struct timeval tv;

    tv.tv_sec = sec;
    tv.tv_usec = 0;
    FD_ZERO (&fdset);
    FD_SET (0, &fdset);

    printf ("[%ld] select_wait (%d) start\n", pthread_self (), sec);
    if (select (FD_SETSIZE, NULL, NULL, NULL, &tv) < 0) {
	fprintf (stderr, "[%ld] select: %s\n", pthread_self (), 
		 strerror (errno));
    }
    printf ("[%ld] select_wait (%d) end\n", pthread_self (), sec);
}


static void 
alarm_interrupt (int sig)
{
    printf ("+++[%ld] signal (%d) received\n", pthread_self (), sig);
    fired++;
}

static void *
child (void *i)
{
    pthread_sigmask (SIG_UNBLOCK, &mysigset, NULL);
    printf ("[%ld] child started\n", pthread_self ());
    select_wait (waiting_time);
    printf ("[%ld] child ended\n", pthread_self ());
    return (NULL);
}

static void *
child_with_alarm (void *i)
{
    pthread_sigmask (SIG_UNBLOCK, &mysigset, NULL);
    printf ("[%ld] child alarm (%d)\n", pthread_self (),
	     waiting_time);
    alarm (waiting_time);
    select_wait (waiting_time);
    return (NULL);
}

static void *
child_sigwait (void *i)
{
    int sig;

    /* wait on alarm */
    if (sigwait (&mysigset, &sig) < 0) {
        fprintf (stderr, "[%ld] sigwait: %s\n", 
		 pthread_self (), strerror (errno));
    }
    else
        printf ("+++ [%ld] sigwait (%d)\n", pthread_self (), sig);
    fired++;
    return (NULL);
}


int
main ()
{
    pthread_t thread1, thread2, thread3;
    int i;

    signal (SIGINT, alarm_interrupt);
    signal (SIGHUP, alarm_interrupt);
    signal (SIGPIPE, alarm_interrupt);

    sigemptyset (&mysigset);
    sigaddset (&mysigset, SIGALRM);
    sigaddset (&mysigset, SIGHUP);
    sigaddset (&mysigset, SIGINT);
    sigaddset (&mysigset, SIGPIPE);

    printf ("*** test #1 (main alarm)\n");
    fired = 0;
    pthread_sigmask (SIG_UNBLOCK, &mysigset, NULL);
    signal (SIGALRM, alarm_interrupt);
    pthread_create (&thread1, NULL, child, NULL);
    pthread_create (&thread2, NULL, child, NULL);
    pthread_create (&thread3, NULL, child, NULL);

    printf ("[%ld] main alarm (%d)\n", pthread_self (), waiting_time);
    alarm (waiting_time);
    select_wait (waiting_time * 2);
    alarm (0);
    if (fired == 0)
	printf ("### no alarm detected!\n");
    pthread_cancel (thread1);
    pthread_cancel (thread2);
    pthread_cancel (thread3);
    pthread_join (thread1, NULL);
    pthread_join (thread2, NULL);
    pthread_join (thread3, NULL);

    printf ("\n");
    printf ("*** test #1 (main alarm, main block)\n");
    fired = 0;
    pthread_sigmask (SIG_UNBLOCK, &mysigset, NULL);
    signal (SIGALRM, alarm_interrupt);
    pthread_create (&thread1, NULL, child, NULL);
    pthread_create (&thread2, NULL, child, NULL);
    pthread_create (&thread3, NULL, child, NULL);

    pthread_sigmask (SIG_BLOCK, &mysigset, NULL);
    printf ("[%ld] main alarm (%d)\n", pthread_self (), waiting_time);
    alarm (waiting_time);
    select_wait (waiting_time * 2);
    pthread_cancel (thread1);
    pthread_cancel (thread2);
    pthread_cancel (thread3);
    pthread_join (thread1, NULL);
    pthread_join (thread2, NULL);
    pthread_join (thread3, NULL);
    alarm (0);
    if (fired == 0)
	printf ("### no alarm detected!\n");

    printf ("\n");
    printf ("*** test #2 (child alarm)\n");
    fired = 0;
    pthread_sigmask (SIG_UNBLOCK, &mysigset, NULL);
    signal (SIGALRM, alarm_interrupt);
    pthread_create (&thread1, NULL, child, NULL);
    pthread_create (&thread1, NULL, child, NULL);
    pthread_create (&thread3, NULL, child_with_alarm, NULL);
    select_wait (waiting_time * 2);
    pthread_cancel (thread1);
    pthread_cancel (thread2);
    pthread_cancel (thread3);
    pthread_join (thread1, NULL);
    pthread_join (thread2, NULL);
    pthread_join (thread3, NULL);
    alarm (0);
    if (fired == 0)
	printf ("### no alarm detected!\n");

    printf ("\n");
    printf ("*** test #2 (child alarm, main block)\n");
    pthread_sigmask (SIG_UNBLOCK, &mysigset, NULL);
    signal (SIGALRM, alarm_interrupt);
    pthread_create (&thread1, NULL, child, NULL);
    pthread_create (&thread2, NULL, child, NULL);
    pthread_sigmask (SIG_BLOCK, &mysigset, NULL);
    pthread_create (&thread3, NULL, child_with_alarm, NULL);
    select_wait (waiting_time * 2);
    pthread_cancel (thread1);
    pthread_cancel (thread2);
    pthread_cancel (thread3);
    pthread_join (thread1, NULL);
    pthread_join (thread2, NULL);
    pthread_join (thread3, NULL);
    alarm (0);
    if (fired == 0)
	printf ("### no alarm detected!\n");

    printf ("\n");
    printf ("*** test #3 (child sigwait)\n");
    fired = 0;
    pthread_sigmask (SIG_UNBLOCK, &mysigset, NULL);
    signal (SIGALRM, alarm_interrupt);
    pthread_create (&thread1, NULL, child_sigwait, NULL);
    pthread_create (&thread2, NULL, child_sigwait, NULL);
    pthread_create (&thread3, NULL, child_sigwait, NULL);
    printf ("[%ld] main alarm (%d)\n", pthread_self (), waiting_time);
    alarm (waiting_time);
    select_wait (waiting_time * 2);
    pthread_cancel (thread1);
    pthread_cancel (thread2);
    pthread_cancel (thread3);
    pthread_join (thread1, NULL);
    pthread_join (thread2, NULL);
    pthread_join (thread3, NULL);
    alarm (0);
    if (fired == 0)
	printf ("### no alarm detected!\n");

    printf ("\n");
    printf ("*** test #3 (child sigwait, main block)\n");
    fired = 0;
    pthread_sigmask (SIG_UNBLOCK, &mysigset, NULL);
    signal (SIGALRM, alarm_interrupt);
    pthread_create (&thread1, NULL, child_sigwait, NULL);
    pthread_create (&thread2, NULL, child_sigwait, NULL);
    pthread_create (&thread3, NULL, child_sigwait, NULL);
    pthread_sigmask (SIG_BLOCK, &mysigset, NULL);
    printf ("[%ld] main alarm (%d) test\n", pthread_self (), waiting_time);
    alarm (waiting_time);
    select_wait (waiting_time * 2);
    pthread_cancel (thread1);
    pthread_cancel (thread2);
    pthread_cancel (thread3);
    pthread_join (thread1, NULL);
    pthread_join (thread2, NULL);
    pthread_join (thread3, NULL);
    alarm (0);
    if (fired == 0)
	printf ("### no alarm detected!\n");

    return (0);
}
#endif /* HAVE_LIBPTHREAD */
