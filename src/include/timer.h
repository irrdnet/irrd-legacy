/*
 * $Id: timer.h,v 1.3 2001/08/09 20:03:54 ljb Exp $
 */

#ifndef _TIMER_H
#define _TIMER_H

#include <sys/types.h>
#include <signal.h>
#include <New.h>
#include <linked_list.h>
#include <schedule.h>

#include <mrt_thread.h>
#if !defined(HAVE_SIGPROCMASK)
typedef unsigned long sigset_t;
#endif /* HAVE_SIGPROCMASK */

#define TIMER_ONE_SHOT	0x0001 /* a one-shot timer (i.e. not periodic) */
#define TIMER_EXPONENT	0x0002 /* exponentially increase */
#define TIMER_PUSH_OWNARG 0x0004 /* internally used to be compatible */
#define TIMER_AUTO_DELETE 0x0008 /* deleted after fire (implies one shot) */
#define TIMER_JITTER1	0x0010
#define TIMER_JITTER2	0x0020
#define TIMER_EXPONENT_SET 0x0040
#define TIMER_EXPONENT_MAX 0x0080
#define TIMER_COUNTER_SET 0x0100

typedef struct _mtimer_t {
    int time_interval;	/* seconds between firing (plus jitter and exponent) */ 
    int time_interval_base;	/* seconds between firing base */
    int time_interval_exponent; /* exponent */
    int time_interval_exponent_max; /* exponent */
    time_t time_next_fire;	/* real time after which should fire */
    int time_jitter_low;	/* 0 no jitter, low bound % of the value */
    int time_jitter_high;	/* 0 no jitter, high bound % of the value */
    int jitter;		/* 0 no jitter, else random abs between +/- value */
    int time_interval_count;	/* counter */
#ifdef notdef
    void (*call_fn)();		/* function to call when timer fires */
    void *arg;			/* arguments to give to called function */
#else
    event_t *event;
#endif
    schedule_t *schedule;	/* schedule the event instead of calling it */
    /* int on; */		/* timer is turned ON if this set to 1 */
    char *name;			/* used for logging and debugging */
    u_long flags;		/* one-shot, etc */
} mtimer_t;


typedef struct _Timer_Master {
   time_t time_interval; /* seconds between firing (debugging purposes) */
   time_t time_next_fire;	/* real time after which should fire */
   pthread_mutex_t mutex_lock;
   LINKED_LIST *ll_timers;	/* linked list of period timers */
   trace_t *trace;
   /* sigset_t sigmask; */
} Timer_Master;

extern Timer_Master *TIMER_MASTER;

void init_timer ();
/*
void Timer_Master_Fire ();
void Timer_Master_Update (mtimer_t *timer);
int Timer_Compare (mtimer_t *t1, mtimer_t *t2);
void Destroy_Timer_Master (Timer_Master *timer);
*/
void Timer_Turn_OFF (mtimer_t *timer);
void Timer_Turn_ON (mtimer_t *timer);
mtimer_t *New_Timer (void (*call_fn)(), int interval, char *name, void *arg);
mtimer_t *New_Timer2 (char *name, int interval, u_long flags, 
		      schedule_t *schedule, void (*call_fn)(), int narg, ...);
void Destroy_Timer (mtimer_t *timer);
void Timer_Set_Time (mtimer_t *timer, int interval);
void Timer_Reset_Time (mtimer_t *timer);
void Timer_Adjust_Time (mtimer_t *timer, int interval);

void timer_set_flags (mtimer_t *timer, u_long flags, ...);
void timer_set_jitter (mtimer_t *timer, int jitter);
void timer_set_jitter2 (mtimer_t *timer, int low, int high);
int time_left (mtimer_t *timer);

/*
sigset_t block_signal (int sig);
void recover_signal (sigset_t old);
*/
void mrt_alarm (void);
void init_signal (trace_t *tr, void_fn_t fn);

#endif /* _TIMER_H */
