/*
 * $Id: mrt_thread.h,v 1.3 2001/08/09 20:02:49 ljb Exp $
 */

#ifndef _MRT_THREAD_H
#define _MRT_THREAD_H

#include "config.h"

#ifndef HAVE_LIBPTHREAD
#include "pthread_fake.h"
#else
#ifdef HAVE_PTHREAD_H
#include <pthread.h>
#endif /* HAVE_PTHREAD_H */
#endif /* HAVE_LIBPTHREAD */

#if !defined(HAVE_SIGPROCMASK)
typedef unsigned long sigset_t;
#endif /* HAVE_SIGPROCMASK */

#ifndef HAVE_PTHREAD_ATTR_SETSCOPE
#define pthread_attr_setscope(attr, scope) /* nothing */
#endif /* HAVE_PTHREAD_ATTR_SETSCOPE */

#endif /* _MRT_THREAD_H */
