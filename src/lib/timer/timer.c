/*
 * $Id: timer.c,v 1.2 2000/08/04 04:14:16 labovit Exp $
 */

#include <mrt.h>
#include <timer.h>


/* #define DEBUG_TIMER	1 */
 
Timer_Master *TIMER_MASTER;


/*
 * return some random number based on timer jitter values and rand()
 * Jitter values -20 and +30 generates a value between -20% and +30% of
 * the base interval.
 */
static int
timer_jitter (mtimer_t *timer)
{
    int range;
    int value = 0;
  
    /* old interface to set a jitter +/- abosolute value */
    if (timer->jitter > 0) {
	value = (rand() % (timer->jitter * 2 + 1)) - timer->jitter;
    }

    /* new way */
    if ((range = timer->time_jitter_high - timer->time_jitter_low) > 0) {
         value += (timer->time_interval_base * 
	          (timer->time_jitter_low + (rand () % (range + 1))) / 100);
    }
    return (value);
}


/*
 * return the number of seconds left before timer will fire
 */
int
time_left (mtimer_t *timer)
{
    assert (timer);
    return (timer->time_next_fire - time (NULL));
}
  

/*
 * Compare two timers for use in sorting. A timer that is 
 * ON always comes before OFF timers
 */
static int
Timer_Compare (mtimer_t *t1, mtimer_t *t2)
{

#ifdef DEBUG_TIMER
    printf("SORTING: Comparing %s:%d and %s:%d\n",
	   t1->name, t1->time_next_fire,
	   t2->name, t2->time_next_fire);
#endif
   
    if (t1->time_next_fire == 0 && t2->time_next_fire != 0)
         return (1);
    if (t1->time_next_fire != 0 && t2->time_next_fire == 0)
         return (-1);

    return (t1->time_next_fire - t2->time_next_fire);
}

 
/*
 * Update global TIMER_MASTER structure with new, or changed timer. 
 * if not given, sort anyway. Resort timer list and
 * change next_fire if timer fire before, and set alarm.
 */
static void
Timer_Master_Update (mtimer_t *timer)
{

    /* should be protected */
    assert (pthread_mutex_trylock (&TIMER_MASTER->mutex_lock));

    if (LL_GetCount (TIMER_MASTER->ll_timers) <= 0)
	return;

    /* sort the timers */
    if (timer)
	LL_ReSort (TIMER_MASTER->ll_timers, timer);

    timer = LL_GetHead (TIMER_MASTER->ll_timers);

    if (timer->time_next_fire == 0) {
        if (TIMER_MASTER->time_next_fire) {
            TIMER_MASTER->time_interval = 0;
            TIMER_MASTER->time_next_fire = 0;
            alarm (0);
            trace (TR_TIMER, TIMER_MASTER->trace, 
		   "Timer_Master stopped running\n");
        }
        return;
    }

    if (TIMER_MASTER->time_next_fire <= 0 /* not running */||
            TIMER_MASTER->time_next_fire > timer->time_next_fire) {
	time_t seconds;

        TIMER_MASTER->time_next_fire = timer->time_next_fire;
        TIMER_MASTER->time_interval = timer->time_interval;

	assert (TIMER_MASTER->time_next_fire > 0);

        seconds = TIMER_MASTER->time_next_fire - time (NULL);
        if (seconds <= 0)
            seconds = 1;
        alarm (seconds);
        trace (TR_TIMER, TIMER_MASTER->trace, 
	       "Timer_Master will be fired in %d seconds\n", seconds);
    }
}


/*
 * Multiplex alarm signal to list of timers. 
 * Fire timers if next fire time before current time
 */
static void
Timer_Master_Fire (void)
{
    mtimer_t *timer;
    time_t now;
    LINKED_LIST *ll = NULL;
    int fired = 0;

#ifdef DEBUG_TIMER
    printf ("Master TIMER FIRE!\n");
#endif
    pthread_mutex_lock (&TIMER_MASTER->mutex_lock);
    time (&now);
    assert (LL_GetCount (TIMER_MASTER->ll_timers));
    TIMER_MASTER->time_next_fire = 0;

    LL_Iterate (TIMER_MASTER->ll_timers, timer) {

        if (timer->time_next_fire == 0 || timer->time_next_fire > now)
	    break; /* assume that they are sorted correctly */

	fired++;

	if (timer->schedule) {
	    /* schedule it right now while locking */
	    schedule_event3 (timer->schedule, Ref_Event (timer->event));
	}
	else {
	    /* delay it because execution time is unknown 
	       and more impotantly the function may call timer routines
	       that requires the lock (deadlock happens) */
	    LL_Add2 (ll, Ref_Event (timer->event));
	}

        if (BIT_TEST (timer->flags, TIMER_AUTO_DELETE)) {
	    mtimer_t *prev = LL_GetPrev (TIMER_MASTER->ll_timers, timer);
	    LL_RemoveFn (TIMER_MASTER->ll_timers, timer, NULL);
	    Deref_Event (timer->event);
	    if (timer->name)
    	        Delete (timer->name);
    	    Delete (timer);
	    timer = prev;
	}
        else if (BIT_TEST (timer->flags, TIMER_ONE_SHOT)) {
             timer->time_next_fire = 0;
        }
        else {
	    timer->time_interval = timer->time_interval_base;
	
            if (BIT_TEST (timer->flags, TIMER_EXPONENT)) {
		/* I don't care of overflow because it's not realistic */
	        timer->time_interval <<= timer->time_interval_exponent;
		if (timer->time_interval_exponent < 
			timer->time_interval_exponent_max)
		    timer->time_interval_exponent++;
	    }
	    timer->time_interval += timer_jitter (timer);
	    timer->time_next_fire += timer->time_interval;
    	}
    }

    /* unlock timer_master before calling their functions */
    pthread_mutex_unlock (&TIMER_MASTER->mutex_lock);

    if (ll) {
	event_t *event;
        /* okay, finally call timer functions */
        LL_Iterate (ll, event) {
	    /* this destroy the event */
	    schedule_event_dispatch (event);
        }
        LL_Destroy (ll);
    }

    /* lock timer_master again to get time for the next fire */
    /* this is separated since the above function calls may destroy the timer */
    pthread_mutex_lock (&TIMER_MASTER->mutex_lock);
    /* check to see if the master timer is updated */
    if (TIMER_MASTER->time_next_fire) {
        pthread_mutex_unlock (&TIMER_MASTER->mutex_lock);
	return;
    }

    if (fired)
        LL_Sort (TIMER_MASTER->ll_timers);
    Timer_Master_Update (NULL);
    pthread_mutex_unlock (&TIMER_MASTER->mutex_lock);
}


/*
 * Allocate memory and initialize global TIMER_MASTER variable
 */
void
init_timer (trace_t *tr)
{
    assert (TIMER_MASTER == NULL);

    TIMER_MASTER = New (Timer_Master);
    TIMER_MASTER->ll_timers = LL_Create (LL_CompareFunction, Timer_Compare,
			                 LL_AutoSort, True, 0);
    TIMER_MASTER->time_interval = 0;
    TIMER_MASTER->time_next_fire = 0;
    TIMER_MASTER->trace = trace_copy (tr);
    set_trace (TIMER_MASTER->trace, TRACE_PREPEND_STRING, "TIMER", 0);
    pthread_mutex_init (&TIMER_MASTER->mutex_lock, NULL);
    init_signal (tr, Timer_Master_Fire);
}


#ifdef notdef
void 
Destroy_Timer_Master (Timer_Master *timer)
{
    assert (timer);
    LL_Destroy (timer->ll_timers);
    Destroy (timer);
}
#endif


/*
 * Turn a timer off and update the TIMER_MASTER info
 */
void 
Timer_Turn_OFF (mtimer_t *timer)
{
    assert (timer);
   
    if (timer->time_next_fire > 0) {
        pthread_mutex_lock (&TIMER_MASTER->mutex_lock);
        timer->time_next_fire = 0;
        Timer_Master_Update (timer);
        trace (TR_TIMER, TIMER_MASTER->trace, "Timer %s OFF\n", timer->name);
        pthread_mutex_unlock (&TIMER_MASTER->mutex_lock);
    }
}


/*
 * Reset timer to fire in timer->time_interval and 
 * notify TIMER_MASTER of change.
 */

void 
Timer_Reset_Time (mtimer_t *timer)
{
    assert (timer);

    if (timer->time_interval_base == 0 && timer->jitter == 0) {
	Timer_Turn_OFF (timer);
	return;
    }

    timer->time_interval = timer->time_interval_base;
    if (BIT_TEST (timer->flags, TIMER_EXPONENT)) {
	/* I don't care of overflow because it's not realistic */
        timer->time_interval <<= timer->time_interval_exponent;
	if (timer->time_interval_exponent < 
		timer->time_interval_exponent_max)
	    timer->time_interval_exponent++;
    }
    timer->time_interval += timer_jitter (timer);

    pthread_mutex_lock (&TIMER_MASTER->mutex_lock);
    /* time_next_fire is a key to sort so that it must be protected */
    timer->time_next_fire = time (NULL) + timer->time_interval;
    trace (TR_TIMER, TIMER_MASTER->trace, "Timer %s restarted with %d\n", 
           timer->name, timer->time_interval);
    Timer_Master_Update (timer);
    pthread_mutex_unlock (&TIMER_MASTER->mutex_lock);
}


/*
 * Turn a timer ON and update TIMER_MASTER info
 */
void 
Timer_Turn_ON (mtimer_t *timer)
{
    Timer_Reset_Time (timer);
}


/*
 * Change intreval of timer and notify TIMER_MASTER of new info.
 * never starts the timer
 */
void
Timer_Set_Time (mtimer_t *timer, int interval)
{
    assert (timer);
    trace (TR_TIMER, TIMER_MASTER->trace, 
	   "Timer %s interval %d changed to %d\n", 
           timer->name, timer->time_interval_base, interval);
    timer->time_interval_base = interval;
    if (timer->time_next_fire > 0 /* running */)
        Timer_Reset_Time (timer);
} 


/* Unlike Timer_Set_Time, if running, adjust the next fire time */
void
Timer_Adjust_Time (mtimer_t *timer, int interval)
{
    assert (timer);
    trace (TR_TIMER, TIMER_MASTER->trace, 
	   "Timer %s interval %d adjusted to %d\n", 
           timer->name, timer->time_interval_base, interval);
    timer->time_interval_base = interval;
    if (timer->time_next_fire > 0 /* running */) {
	/* adjust next fire time */
	timer->time_next_fire += (interval - timer->time_interval);
        pthread_mutex_lock (&TIMER_MASTER->mutex_lock);
        Timer_Master_Update (timer);
        pthread_mutex_unlock (&TIMER_MASTER->mutex_lock);
    }
} 


void 
timer_set_flags (mtimer_t *timer, u_long flags, ...)
{
    va_list ap;

    BIT_SET (timer->flags, flags);
    va_start (ap, flags);

    /* these are a new way so that the code may not take care of these flags,
       but it works as it was */
    switch (flags) {
    case TIMER_JITTER1:
	timer->jitter = va_arg (ap, int);
        break;
    case TIMER_JITTER2:
	timer->time_jitter_low = va_arg (ap, int);
        timer->time_jitter_high = va_arg (ap, int);
        break;
    case TIMER_EXPONENT_SET:
        timer->time_interval_exponent = va_arg (ap, int);
        break;
    case TIMER_EXPONENT_MAX:
        timer->time_interval_exponent_max = va_arg (ap, int);
        break;
    default:
	break;
    }
}


void
timer_set_jitter (mtimer_t *timer, int jitter)
{
    if (jitter != 0)
        BIT_SET (timer->flags, TIMER_JITTER1);
    timer->jitter = jitter;
}


void
timer_set_jitter2 (mtimer_t *timer, int low, int high)
{
    if (low != high)
        BIT_SET (timer->flags, TIMER_JITTER2);
    timer->time_jitter_low = low;
    timer->time_jitter_high = high;
}


/*
 * Allocate memory and initialize a mtimer_t structure. 
 * Also add new mtimer_t info to global TIMER_MASTER
 * NOTE: timers are created OFF!
 * if there is schedule, the function will be scheduled to be called later
 */
mtimer_t *
New_Timer2 (char *name, int interval, u_long flags, schedule_t *schedule, 
	    event_fn_t call_fn, int narg, ...)
{
    mtimer_t *timer;
    va_list ap;
    event_t *event;
    int i = 0;

    timer = New (mtimer_t);
/*
    timer->call_fn = call_fn;
    timer->arg = arg;
*/
    timer->time_interval_base = interval;
    timer->time_interval = 0;
    timer->time_interval_exponent = 0;
    timer->time_interval_exponent_max = 0;
    timer->time_next_fire = 0; /* off */
    timer->time_jitter_low = 0;
    timer->time_jitter_high = 0;
    timer->name = (name)? strdup (name): NULL;
    timer->flags = flags;

    va_start (ap, narg);

    if (BIT_TEST (flags, TIMER_PUSH_OWNARG))
	narg++;
    event = New_Event (narg);
    /* TIMER_PUSH_OWNARG & TIMER_AUTO_DELETE have to be set on creation */

    event->description = (timer->name)? strdup (timer->name): NULL;

    if (BIT_TEST (flags, TIMER_PUSH_OWNARG))
        event->args[i++] = timer;
    for (;i < narg; i++)
       event->args[i] = va_arg (ap, void *);

    event->call_fn = call_fn;

    va_end (ap);
    timer->event = event;
    timer->schedule = schedule;

    pthread_mutex_lock (&TIMER_MASTER->mutex_lock);
    LL_Add (TIMER_MASTER->ll_timers, timer);
    Timer_Master_Update (NULL); /* already sorted */
    pthread_mutex_unlock (&TIMER_MASTER->mutex_lock);
    return (timer);
}


/* old interface with only one argument */
mtimer_t *
New_Timer (event_fn_t call_fn, int interval, char *name, void *arg)
{
    return (New_Timer2 (name, interval, TIMER_PUSH_OWNARG, NULL, 
			call_fn, 1, arg));
}


void
Destroy_Timer (mtimer_t *timer)
{
    Timer_Turn_OFF (timer);
    pthread_mutex_lock (&TIMER_MASTER->mutex_lock);
    LL_Remove (TIMER_MASTER->ll_timers, timer);
    pthread_mutex_unlock (&TIMER_MASTER->mutex_lock);

    Deref_Event (timer->event);
    if (timer->name)
        Delete (timer->name);
    Delete (timer);
}
