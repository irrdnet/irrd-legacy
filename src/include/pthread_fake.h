/* 
 * $Id: pthread_fake.h,v 1.1.1.1 2000/02/29 22:28:43 labovit Exp $
 */

/* This is a compatability include file for systems that DO NOT 
 * has kernel pthreads or which are NOT using MIT pthreads library
 * This file just defines pthread functions as null routines
 */

#ifndef _PTHREADS_FAKE_H
#define _PTHREADS_FAKE_H

/* since SUNOS 5.6, pthread structures are defined in sys/types.h,
   which conflists with the following even when pthread is disabled */
typedef	int	mrt_pthread_t;
typedef int	mrt_pthread_attr_t;
typedef struct { int value; char *file; int line; } mrt_pthread_mutex_t;
typedef struct { int value; char *file; int line; } mrt_pthread_cond_t;
typedef int	mrt_pthread_mutexattr_t;
typedef int	mrt_pthread_condattr_t;

#define pthread_t mrt_pthread_t
#define pthread_attr_t mrt_pthread_attr_t
#define pthread_mutex_t mrt_pthread_mutex_t
#define pthread_cond_t mrt_pthread_cond_t
#define pthread_mutexattr_t mrt_pthread_mutexattr_t
#define pthread_condattr_t mrt_pthread_condattr_t

#define pthread_self() get_thread_id()

#define pthread_mutex_init(a, b)  mrt_pthread_mutex_init(a,b,__FILE__,__LINE__)
#define pthread_mutex_lock(a)	  mrt_pthread_mutex_lock(a,__FILE__,__LINE__)
#define pthread_mutex_trylock(a)  mrt_pthread_mutex_trylock(a,__FILE__,__LINE__)
#define pthread_mutex_unlock(a)	  mrt_pthread_mutex_unlock(a,__FILE__,__LINE__)
#define pthread_mutex_destroy(a)  mrt_pthread_mutex_destroy(a,__FILE__,__LINE__)
#define pthread_cond_init(a,b)	  mrt_pthread_cond_init(a,b,__FILE__,__LINE__)
#define pthread_cond_wait(a,b)	  mrt_pthread_cond_wait(a,b,__FILE__,__LINE__)
#define pthread_cond_signal(a)	  mrt_pthread_cond_signal(a,__FILE__,__LINE__)
#define pthread_cond_destroy(a)	  mrt_pthread_cond_destroy(a,__FILE__,__LINE__)
#define pthread_sigmask(a,b,c)	  sigprocmask(a,b,c)
#define pthread_init()		  0
#define pthread_attr_init(a)	  (*(a) = 0)

int mrt_pthread_mutex_init (pthread_mutex_t *mutex, 
			const pthread_mutexattr_t *attr, char *file, int line);
int mrt_pthread_mutex_lock (pthread_mutex_t *mutex, char *file, int line);
int mrt_pthread_mutex_trylock (pthread_mutex_t *mutex, char *file, int line);
int mrt_pthread_mutex_unlock (pthread_mutex_t *mutex, char *file, int line);
int mrt_pthread_mutex_destroy (pthread_mutex_t *mutex, char *file, int line);
int mrt_pthread_cond_init (pthread_cond_t *cond, 
		const pthread_condattr_t *attr, char *file, int line);
int mrt_pthread_cond_wait (pthread_cond_t *cond, pthread_mutex_t *mutex, 
		char *file, int line);
int mrt_pthread_cond_signal (pthread_cond_t *cond, char *file, int line);
int mrt_pthread_cond_destroy (pthread_cond_t *cond, char *file, int line);

#define PTHREAD_MUTEX_INITIALIZER	{0, NULL, 0}

#endif /* _PTHREADS_FAKE_H */
