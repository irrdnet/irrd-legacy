/*
 * $Id: schedule.c,v 1.2 2001/08/06 17:01:50 ljb Exp $
 */		

#include <mrt.h>

schedule_master_t *SCHEDULE_MASTER;

/* #define MRT_DEBUG 1 */

int 
init_schedules (trace_t *tr)
{
  assert (SCHEDULE_MASTER == NULL);
  SCHEDULE_MASTER = New (schedule_master_t);
  SCHEDULE_MASTER->ll_schedules = LL_Create (0);
  SCHEDULE_MASTER->trace = tr;
  pthread_mutex_init (&SCHEDULE_MASTER->mutex_lock, NULL);
  return (1);
}

schedule_t *
New_Schedule (char *description, trace_t *tr)
{
   schedule_t *schedule;

   schedule = New (schedule_t);
   schedule->ll_events = LL_Create (LL_DestroyFunction, Deref_Event, 0);

   if (description == NULL)
      description = "unknown";
   schedule->description = strdup (description);

   /* this trace condition is delegated from the caller */
   schedule->trace = tr;
   schedule->lastrun = 0;
   schedule->maxnum = 0;
   schedule->is_running = 0;
   schedule->can_pass = 0;

#ifdef HAVE_LIBPTHREAD
   pthread_cond_init (&schedule->cond_new_event, NULL);
#endif /* HAVE_LIBPTHREAD */
   pthread_mutex_init (&schedule->mutex_cond_lock, NULL);
   pthread_mutex_init (&schedule->mutex_lock, NULL);

   pthread_mutex_lock (&SCHEDULE_MASTER->mutex_lock);
   LL_Add (SCHEDULE_MASTER->ll_schedules, schedule);
   pthread_mutex_unlock (&SCHEDULE_MASTER->mutex_lock);

   return (schedule);
}

event_t *
New_Event (int narg)
{
    event_t *event;

    event = New (event_t);
    event->args = (narg > 0)? NewArray (void *, narg): NULL;
    event->narg = narg;
    event->ref_count = 1;
    pthread_mutex_init (&event->mutex_lock, NULL);
    return (event);
}

event_t *
Ref_Event (event_t *event)
{
    pthread_mutex_lock (&event->mutex_lock);
    assert (event->ref_count > 0);
    event->ref_count++;
    pthread_mutex_unlock (&event->mutex_lock);
    return (event);
}

void
Deref_Event (event_t *event)
{
    pthread_mutex_lock (&event->mutex_lock);
    assert (event->ref_count > 0);
    event->ref_count--;
    if (event->ref_count <= 0) {
        if (event->args)
            Delete (event->args);
        pthread_mutex_destroy (&event->mutex_lock);
	if (event->description)
	    Delete (event->description);
        Delete (event);
	return;
    }
    pthread_mutex_unlock (&event->mutex_lock);
}

int
schedule_event3 (schedule_t *schedule, event_t *event)
{
   int n;

   /* obtain lock */
   pthread_mutex_lock (&schedule->mutex_cond_lock);

   LL_Add (schedule->ll_events, event);
   schedule->new_event_flag++;

   n = LL_GetCount (schedule->ll_events);
   if (schedule->maxnum < n)
	schedule->maxnum = n;

#ifdef HAVE_LIBPTHREAD
#ifdef MRT_DEBUG
   trace (TR_THREAD, schedule->trace, "pthread_cond_signal for a new event\n");
#endif /* MRT_DEBUG */
   pthread_cond_signal (&schedule->cond_new_event);
#endif /* HAVE_LIBPTHREAD */

#ifdef MRT_DEBUG
   if (event->description)
      trace (TR_THREAD, schedule->trace,
          "THREAD Event %s scheduled (%d events) for %s\n",
          event->description, n, schedule->description);
   else
      trace (TR_THREAD, schedule->trace,
          "THREAD Event %x scheduled (%d events) for %s\n",
          event->call_fn, n, schedule->description);
#endif /* MRT_DEBUG */

   /* release lock */
   pthread_mutex_unlock (&schedule->mutex_cond_lock);

#ifdef MRT_DEBUG
   if (n > 5000) {
      trace (TR_THREAD, schedule->trace, "THREAD Over 5000 events !!!!!!");
      trace (TR_THREAD, schedule->trace, "THREAD *** SCHEDULE OVERFLOW ***");
      return (-1);
   }
#endif /* MRT_DEBUG */
   
   return (1);
}

static int
schedule_event2ap (char *description, schedule_t *schedule, 
		   pthread_mutex_t *mutex, pthread_cond_t *cond, 
		   int *ret,
		   event_fn_t call_fn, int narg, va_list ap) {

   event_t *event;
   int i;

   event = New_Event (narg);

   /* I expect that description is in a static strage */
   if (description)
       event->description = strdup (description);
    else
       event->description = strdup ("?");

   for (i = 0; i < narg; i++)
      event->args[i] = va_arg (ap, void *);

   event->call_fn = call_fn;
   event->mutex = mutex;
   event->cond = cond;
   event->ret = ret;

   return (schedule_event3 (schedule, event));
}

int 
schedule_event_and_wait (char *description, schedule_t *schedule, 
		 	 event_fn_t call_fn, int narg, ...)
{
   va_list ap;
   int ret;
   pthread_mutex_t mutex;
   pthread_cond_t cond;

   pthread_mutex_init (&mutex, NULL);
   pthread_cond_init (&cond, NULL);
   pthread_mutex_lock (&mutex);
   va_start (ap, narg);
   schedule_event2ap (description, schedule, &mutex, &cond, 
		      &ret, call_fn, narg, ap);
   va_end (ap);
   pthread_cond_wait (&cond, &mutex);
   pthread_mutex_destroy (&mutex);
   pthread_cond_destroy (&cond);
   return (ret);
}

int 
schedule_event2 (char *description, schedule_t *schedule, 
		 event_fn_t call_fn, int narg, ...)
{
   va_list ap;
   int ret;

   va_start (ap, narg);
   ret = schedule_event2ap (description, schedule, NULL, NULL, 
			    NULL, call_fn, narg, ap);
   va_end (ap);
   return (ret);
}


/* old interface without description
   for compatibility purpose only */
int
schedule_event (schedule_t *schedule, event_fn_t call_fn, int narg, ...)
{
   va_list ap;
   int ret;

   va_start (ap, narg);
   ret = schedule_event2ap (NULL, schedule, NULL, NULL, NULL, call_fn, narg, ap);
   va_end (ap);
   return (ret);
}

int
call_args_list (int_fn_t call_fn, int narg, void **args)
{
   int ret = -1;
   switch (narg) {
      case 0:
         ret = call_fn ();
	 break;
      case 1:
         ret = call_fn (args[0]);
	 break;
      case 2:
         ret = call_fn (args[0], args[1]);
	 break;
      case 3:
         ret = call_fn (args[0], args[1], args[2]);
	 break;
      case 4:
         ret = call_fn (args[0], args[1], args[2], args[3]);
	 break;
      case 5:
         ret = call_fn (args[0], args[1], args[2], args[3], args[4]);
	 break;
      case 6:
         ret = call_fn (args[0], args[1], args[2], args[3], args[4],
		  args[5]);
	 break;
      case 7:
         ret = call_fn (args[0], args[1], args[2], args[3], args[4], 
		  args[5], args[6]);
	 break;
      case 8:
         ret = call_fn (args[0], args[1], args[2], args[3], args[4], 
		  args[5], args[6], args[7]);
	 break;
      case 9:
         ret = call_fn (args[0], args[1], args[2], args[3], args[4], 
		  args[5], args[6], args[7], args[8]);
	 break;
      case 10:
         ret = call_fn (args[0], args[1], args[2], args[3], args[4], 
		  args[5], args[6], args[7], args[8], args[9]);
	 break;
      default:
	 /* XXX */
	 trace (TR_FATAL, MRT->trace, 
		"Please code here file = %s, line = %d\n", __FILE__, __LINE__);
         break;
    }
    return (ret);
}

void
schedule_event_dispatch (event_t *event)
{
   int ret;

   if (event->mutex)
      pthread_mutex_lock (event->mutex);

    ret = call_args_list ((int_fn_t) event->call_fn, event->narg, event->args);

   if (event->ret)
      *event->ret = ret;
   if (event->cond)
      pthread_cond_signal (event->cond);
   if (event->mutex)
      pthread_mutex_unlock (event->mutex);
   Deref_Event (event);
}

#ifdef HAVE_LIBPTHREAD
int 
schedule_wait_for_event (schedule_t *schedule)
{
   event_t *event;

   pthread_mutex_lock (&schedule->mutex_cond_lock);

   while (schedule->new_event_flag == 0) {
#ifdef MRT_DEBUG
      trace (TR_THREAD, schedule->trace, "pthread_cond_wait for a new event\n");
#endif /* MRT_DEBUG */
      pthread_cond_wait (&schedule->cond_new_event, 
			 &schedule->mutex_cond_lock);
   }

#ifdef MRT_DEBUG
   trace (TR_THREAD, schedule->trace, "Processing event queue\n");
#endif /* MRT_DEBUG */
   event = LL_GetHead (schedule->ll_events);

   if (event != NULL) {
     LL_RemoveFn (schedule->ll_events, event, NULL);
     schedule->new_event_flag --;
     schedule->lastrun = time (NULL);
   }
   else {
     trace (TR_ERROR, schedule->trace, 
	    "THREAD NULL event for %s -- strange...", 
	    schedule->description);
   }

   /* unlock -- we removed the event from the schedule quere */
   pthread_mutex_unlock (&schedule->mutex_cond_lock);

   if (event == NULL) {return (-1);}

#ifdef MRT_DEBUG
  if (event->description)
    trace (TR_THREAD, schedule->trace, 
      "THREAD Event %s now run (%d events left) for %s\n", 
      event->description, LL_GetCount (schedule->ll_events),
      schedule->description);
  else
    trace (TR_THREAD, schedule->trace, 
      "THREAD Event %x now run (%d events left) for %s\n", 
      event->call_fn, LL_GetCount (schedule->ll_events),
      schedule->description);
#endif /* MRT_DEBUG */

   schedule->is_running++;
   schedule_event_dispatch (event);
   schedule->is_running--;
  if (BIT_TEST (schedule->flags, MRT_SCHEDULE_DELETED))
    destroy_schedule (schedule);
   return (1);
}
#endif /* HAVE_LIBPTHREAD */

#ifdef HAVE_LIBPTHREAD
/* intervene_thread
 * Halt a thread so we can gain access to its private data space.
 * This is a convenience function mainly used if config and show output
 * routines. Easier for the uii or config thread to retain control than 
 * have to negotiate output, etc.
 *
 */
static void 
intervene_thread (pthread_mutex_t *mutex, pthread_mutex_t *mutex_cond_lock, 
		  pthread_cond_t *cond) {

  /* signal cond lock */
  pthread_mutex_lock (mutex_cond_lock);
  pthread_cond_signal (cond);

  /* wait on mutex  -- other process is running now */
  pthread_mutex_lock (mutex);
  
}

pthread_mutex_t *intervene_thread_start (schedule_t *schedule) { 
  pthread_mutex_t *mutex, *mutex_cond_lock;
  pthread_cond_t *cond;
  
  mutex = New (pthread_mutex_t);
  mutex_cond_lock = New (pthread_mutex_t);
  cond = New (pthread_cond_t);


  pthread_mutex_init (mutex, NULL);
  pthread_mutex_init (mutex_cond_lock, NULL);
  pthread_cond_init (cond, NULL);

  /* grab mutex */
  pthread_mutex_lock (mutex);

  pthread_mutex_lock (mutex_cond_lock);
  schedule_event2 ("Intervene", schedule, intervene_thread, 3, 
		   mutex, mutex_cond_lock, cond);
  pthread_cond_wait (cond, mutex_cond_lock);

  /* cleanup */
  pthread_mutex_destroy (mutex_cond_lock);
  
  /* return and do our thing */
  return (mutex);
}

void intevene_thread_end (pthread_mutex_t *mutex) {
  /* unlock mutex */
  pthread_mutex_unlock (mutex);

  pthread_mutex_destroy (mutex);
}
#endif /* HAVE_LIBPTHREAD */

void 
clear_schedule (schedule_t *schedule)
{
   /* obtain lock */
   if (pthread_mutex_trylock (&schedule->mutex_cond_lock) != 0) {
#ifdef MRT_DEBUG
      trace (TR_THREAD, schedule->trace, 
	     "THREAD Going to block while clearing schedule for %s\n",
	     schedule->description);
#endif /* MRT_DEBUG */
      pthread_mutex_lock  (&schedule->mutex_cond_lock);
   }

   LL_Clear (schedule->ll_events);
   schedule->new_event_flag = 0;
#ifdef MRT_DEBUG
   trace (TR_THREAD, schedule->trace, "Cleared schedule\n");
#endif /* MRT_DEBUG */

   pthread_mutex_unlock  (&schedule->mutex_cond_lock);
}

/* remove it from the list */
void 
delete_schedule (schedule_t *schedule)
{
   if (schedule->flags & MRT_SCHEDULE_DELETED)
	return;
   pthread_mutex_lock (&SCHEDULE_MASTER->mutex_lock);
   if (SCHEDULE_MASTER->last_schedule == schedule)
      SCHEDULE_MASTER->last_schedule = 
	 LL_GetNext (SCHEDULE_MASTER->ll_schedules, schedule);
   LL_RemoveFn (SCHEDULE_MASTER->ll_schedules, schedule, 0);
   pthread_mutex_unlock (&SCHEDULE_MASTER->mutex_lock);
#ifdef MRT_DEBUG
   trace (TR_THREAD, schedule->trace, 
	  "THREAD Deleted schedule for %s\n", schedule->description);
#endif /* MRT_DEBUG */
   schedule->flags |= MRT_SCHEDULE_DELETED;
}

/* really free the memory */
void
destroy_schedule (schedule_t *schedule)
{
   if (!BIT_TEST (schedule->flags, MRT_SCHEDULE_DELETED))
	delete_schedule (schedule);
   pthread_mutex_lock  (&schedule->mutex_cond_lock);
   LL_Destroy (schedule->ll_events);
#ifdef MRT_DEBUG
   trace (TR_THREAD, schedule->trace, 
	  "THREAD Destroyed schedule for %s\n", schedule->description);
#endif /* MRT_DEBUG */
   Delete (schedule->description);
#ifdef HAVE_LIBPTHREAD
   pthread_cond_destroy (&schedule->cond_new_event);
#endif /* HAVE_LIBPTHREAD */
   pthread_mutex_destroy (&schedule->mutex_cond_lock);
   pthread_mutex_destroy (&schedule->mutex_lock);
   Delete (schedule);
}

#ifndef HAVE_LIBPTHREAD
int 
process_all_schedules (void)
{
  schedule_t *schedule;
  event_t *event = NULL;
  int i;
  pthread_t save;

  pthread_mutex_lock (&SCHEDULE_MASTER->mutex_lock);
  schedule = SCHEDULE_MASTER->last_schedule;
  for (i = 0; i < LL_GetCount (SCHEDULE_MASTER->ll_schedules); i++) {

    if (schedule == NULL) {
	schedule = LL_GetHead (SCHEDULE_MASTER->ll_schedules);
    }
    else {
        schedule = LL_GetNext (SCHEDULE_MASTER->ll_schedules, schedule);
        if (schedule == NULL)
	    schedule = LL_GetHead (SCHEDULE_MASTER->ll_schedules);
    }

    assert (schedule != NULL);

    if (schedule->is_running > 0 && !schedule->can_pass)
	continue;

    if (schedule->new_event_flag > 0) {
  	pthread_mutex_lock (&schedule->mutex_lock);
        event = LL_GetHead (schedule->ll_events);
        LL_RemoveFn (schedule->ll_events, event, NULL);
        schedule->new_event_flag --;
        schedule->lastrun = time (NULL);
  	pthread_mutex_unlock (&schedule->mutex_lock);
	SCHEDULE_MASTER->last_schedule = schedule;
        break;
    }
  }
  pthread_mutex_unlock (&SCHEDULE_MASTER->mutex_lock);

  if (event == NULL) {
    return (0);
  }

#ifdef MRT_DEBUG
  if (event->description)
    trace (TR_THREAD, schedule->trace, 
      "THREAD Event %s now run (%d events left) for %s\n", 
      event->description, LL_GetCount (schedule->ll_events),
      schedule->description);
  else
    trace (TR_THREAD, schedule->trace, 
      "THREAD Event %x now run (%d events left) for %s\n", 
      event->call_fn, LL_GetCount (schedule->ll_events),
      schedule->description);
#endif /* MRT_DEBUG */

  save = set_thread_id (schedule->self);
  schedule->is_running++;
  schedule_event_dispatch (event);
  schedule->is_running--;
  if (BIT_TEST (schedule->flags, MRT_SCHEDULE_DELETED))
    destroy_schedule (schedule);
  set_thread_id (save);
  return (1);
}

#define THE_FIRST_THREAD_ID 10
static int current_thread_id = THE_FIRST_THREAD_ID;

pthread_t get_thread_id () {
   return (current_thread_id);
}

pthread_t set_thread_id (pthread_t id) {
   pthread_t old_id = current_thread_id;
   current_thread_id = id;
   return (old_id);
}
#endif /* HAVE_LIBPTHREAD */

int schedule_count (schedule_t *schedule) {
  int num;

  if (schedule == NULL) {return (-1);}
  /* obtain lock */
  pthread_mutex_lock (&schedule->mutex_cond_lock);
  num = LL_GetCount (schedule->ll_events);
  pthread_mutex_unlock (&schedule->mutex_cond_lock);

  return (num);
}
