/* 
 * $Id: select.h,v 1.2 2001/07/13 18:10:16 ljb Exp $
 */

#ifndef _SELECT_H
#define _SELECT_H

#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif /* HAVE_SYS_SELECT_H */

#include <linked_list.h>
#include <mrt_thread.h>

/* select type_mask */
#define SELECT_READ		1
#define SELECT_WRITE		2
#define SELECT_EXCEPTION	4

typedef struct _Descriptor_Struct {
   char *name;
   /* int marked_for_deletion; */
   int fd;
   u_long type_mask;	/* SELECT_READ | SELECT_WRITE | SELECT_EXCEPTION */
#ifdef notdef
   void (*call_fn)();
   void *arg;
#else
   event_t *event;
#endif
   schedule_t *schedule;
  time_t	timed;
} Descriptor_Struct;


typedef struct _Select_Struct {
  int		interrupt_fd[2];
  pthread_mutex_t     	mutex_lock;
  pthread_t		self;
  fd_set 	fdvar_read;
  fd_set 	fdvar_write;
  fd_set 	fdvar_except;
  LINKED_LIST 	*ll_descriptors;
  trace_t	*trace;
  LINKED_LIST *ll_close_fds;
} Select_Struct;

extern Select_Struct *SELECT_MASTER;

/* publich functions */
int select_shutdown (void);
int init_select (trace_t *trace);
int start_select (void);
int select_add_fd_event (char *name, int fd, u_long type_mask, int on,
                schedule_t *schedule, event_fn_t call_fn, int narg, ...);
int select_add_fd_event_timed (char *name, int fd, u_long type_mask, int on,
     time_t timed, schedule_t *schedule, event_fn_t call_fn, int narg, ...);
int select_add_fd (int fd, u_long type_mask, void (*call_fn)(), void *arg);
int select_delete_fd (int fd);
int select_delete_fd2 (int fd);
int mrt_select (void);
int mrt_select2 (int ms);
int select_enable_fd (int fd);
int select_enable_fd_mask (int fd, u_long mask);
int select_add_fd_off (int fd, u_long type_mask, void (*call_fn) (), void *arg);
void socket_set_nonblocking (int sockfd, int yes);

#define select_delete_fdx(fd) {if (select_delete_fd(fd) < 0) close(fd);}
#endif /* _SELECT_H */
