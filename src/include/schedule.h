/*
 * $Id: schedule.h,v 1.1.1.1 2000/02/29 22:28:43 labovit Exp $
 */		

#ifndef _SCHEDULE_H
#define _SCHEDULE_H

#include <trace.h>
#include <mrt_thread.h>

typedef void (*event_fn_t)();

typedef struct _event_t {
   char *description;
   event_fn_t call_fn;
   int	narg;
   void **args;
   int *ret;
   pthread_mutex_t *mutex;
   pthread_cond_t *cond;
   int ref_count;
   pthread_mutex_t mutex_lock;
} event_t;


#define MRT_SCHEDULE_DELETED 0x0001
#define MRT_SCHEDULE_RUNNING 0x0002
#define MRT_SCHEDULE_CANPASS 0x0004
#define MRT_SCHEDULE_ATTACHED 0x0008
typedef struct _schedule_t {

   pthread_t 		self;
   pthread_mutex_t 	mutex_lock;
   pthread_mutex_t 	mutex_cond_lock;
#ifdef HAVE_LIBPTHREAD
   pthread_cond_t	cond_new_event;
#endif /* HAVE_LIBPTHREAD */
   int 		is_running;
   int 		can_pass;
   time_t	lastrun;
   int		maxnum;
   int		attached;	/* if thread is attached */

   char 	*description;		/* descrip of schedule for tracing */
   trace_t	*trace;
   int 		new_event_flag;
   LINKED_LIST	*ll_events;
   u_long flags;
} schedule_t;

typedef struct _schedule_master_t {
  LINKED_LIST *ll_schedules;
  schedule_t *last_schedule;
  trace_t	*trace;
  /* lock doesn't need since this seems to be used in non-thread case */
  pthread_mutex_t 	mutex_lock;
} schedule_master_t;

extern schedule_master_t *SCHEDULE_MASTER;

/* public functions */
int init_schedules (trace_t *trace);
schedule_t *New_Schedule (char *description, trace_t *trace);
int schedule_event (schedule_t *schedule, event_fn_t call_fn, 
		    int narg, ...);
int schedule_event2 (char *description, schedule_t *schedule, 
		    event_fn_t call_fn, int narg, ...);
int schedule_event_and_wait (char *description, schedule_t *schedule, 
		    event_fn_t call_fn, int narg, ...);
int schedule_event3 (schedule_t *schedule, event_t *event);
void schedule_event_dispatch (event_t *event);
int schedule_wait_for_event (schedule_t *schedule);
event_t *New_Event (int narg);
event_t *Ref_Event (event_t *event);
void Deref_Event (event_t *event);
void clear_schedule (schedule_t *schedule);
void delete_schedule (schedule_t *schedule);
void destroy_schedule (schedule_t *schedule);
int process_all_schedules ();
int schedule_count (schedule_t *schedule);
int call_args_list (int_fn_t call_fn, int narg, void **args);

#ifndef HAVE_LIBPTHREAD
pthread_t get_thread_id ();
pthread_t set_thread_id (pthread_t id);
#endif /* HAVE_LIBPTHREAD */

#endif /* _SCHEDULE_H */
