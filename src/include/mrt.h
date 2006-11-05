/*
 * $Id: mrt.h,v 1.7 2002/10/17 19:41:44 ljb Exp $
 */

#ifndef _MRT_H
#define _MRT_H

#include <version.h>
#include <config.h>

#include <stdlib.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdio.h>
#ifdef __GNUC__
/* to avoid it defined in stdio.h */
#include <stdarg.h>
#else
#include <sys/varargs.h>
#endif /* __GNUC__ */
#include <errno.h>
#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif
#include <sys/param.h>
#include <sys/socket.h>
#include <unistd.h>
#include <ctype.h>
#ifdef HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif /* HAVE_STRINGS_H */

/* Obtained from GNU autoconf manual */
/* AIX requires this to be the first thing in the file.  */
#ifdef __GNUC__
# ifndef alloca
# define alloca __builtin_alloca
# endif
#else
# if HAVE_ALLOCA_H
#  include <alloca.h>
# else
#  ifdef _AIX
 #pragma alloca
#  else
#   ifndef alloca /* predefined by HP cc +Olibcalls */
char *alloca ();
#   endif
#  endif
# endif
#endif

#include <netinet/in.h>
#include <arpa/inet.h>
#ifdef HAVE_RESOLV_H
#include <arpa/nameser.h>
#include <resolv.h>
#endif /* HAVE_RESOLV_H */

#include <defs.h>
#include <assert.h>
#include <mrt_thread.h>

typedef void (*void_fn_t)();
typedef int (*int_fn_t)();
typedef void *(*thread_fn_t)(void *);

#include <New.h>
#include <linked_list.h>
#include <trace.h>
#include <schedule.h>
#include <select.h>

#ifndef ON
#define ON 1
#endif /* ON */
#ifndef OFF
#define OFF 0
#endif /* OFF */

#define NIL (-1)

#ifndef INET_ADDRSTRLEN
#define INET_ADDRSTRLEN   16
#endif /* INET_ADDRSTRLEN */

#ifndef HAVE_STRUCT_IN6_ADDR
/* IPv6 address */
struct in6_addr {
 u_char s6_addr[16];
};
#endif /* HAVE_STRUCT_IN6_ADDR */

typedef struct _prefix_t {
    u_short family;		/* AF_INET | AF_INET6 */
    u_short bitlen;		/* prefix length */
    int ref_count;		/* reference count */
    pthread_mutex_t mutex_lock; /* lock down structure */
    union {
	struct in_addr sin;
	struct in6_addr sin6;
    } add;
} prefix_t;


/* use to save space */
typedef struct _v4_prefix_t {
    u_short family;		/* AF_INET only */
    u_short bitlen;		/* prefix length */
    int ref_count;		/* reference count */
    pthread_mutex_t mutex_lock; /* lock down structure */
    union {
	struct in_addr sin;
    } add;
} v4_prefix_t;

#include <alist.h>
#include <hash.h>
#include <user.h>
#include <stack.h>

/* Main MRT structure
 * holds bookkeeping information on gateways, threads, signals, etc
 * ALL MRT programs depend on this structure
 */
typedef struct _mrt_t {
    pthread_mutex_t mutex_lock;		/* lock down structure */
    trace_t *trace;			/* default trace - go away future? */
    LINKED_LIST *ll_threads;		/* list of all thread_t */
    LINKED_LIST *ll_trace;		/* list of trace_t */
    char *config_file_name;
    long start_time;			/* uptime of system (debugging) */
#ifndef HAVE_LIBPTHREAD
    /* for use on non-thread systems -- current thread # */
    int threadn;
#endif /* HAVE_LIBPTHREAD */
    /* for rebooting - save cwd, and arguments */
    char	*cwd;
    char        **argv;
    int    	argc;
    int		daemon_mode;
    volatile int force_exit_flag;
#ifndef HAVE_LIBPTHREAD
    int initialization;
#endif /* HAVE_LIBPTHREAD */
    int		pid;
    char	*version;
    char	*date;
} mrt_t;

/* must not the same as any signal number */
#define MRT_FORCE_EXIT   999
#define MRT_FORCE_REBOOT 998
#define MRT_FORCE_ABORT  997

typedef struct _ll_value_t {
    int value;
    LL_POINTERS ptrs;
} ll_value_t;

extern mrt_t *MRT;

/* Main thread gets all signals. Threads can request to have call_fn
 * executed upon receipt of a signal
 */
typedef struct _mrt_signal_t {
    int signal;
    void_fn_t call_fn;
/*    mrt_call_fn_t call_fn; */
} mrt_signal_t;

typedef struct _mrt_thread_t {
    char *name;
    pthread_t thread;
    pthread_attr_t attr;
    schedule_t *schedule; /* schedule sttached to the thread */
} mrt_thread_t;

#ifdef HAVE_LIBPTHREAD

#define THREAD_SPECIFIC_DATA(type, data, size) \
    do { \
        static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER; \
        static pthread_key_t key;\
        static int need_get_key = 1; \
        \
        if (need_get_key) { \
	    pthread_mutex_lock (&mutex); \
	    /* need double check since this thread may be locked */ \
	    if (need_get_key) { \
	        if (pthread_key_create (&key, free) < 0) { \
	            pthread_mutex_unlock (&mutex); \
		    data = (type *) NULL; \
		    perror ("pthread_key_create"); \
		    abort (); \
		    break; \
		} \
		need_get_key = 0; \
	    } \
	    pthread_mutex_unlock (&mutex); \
        } \
        if ((data = (type *) pthread_getspecific (key)) == NULL) { \
	    if ((data = (type *) calloc (size, sizeof (type))) == NULL) { \
		perror ("pthread_getspecific"); \
		abort (); \
	        break; \
	    } \
	    if (pthread_setspecific (key, data) < 0) { \
		perror ("pthread_setspecific"); \
		abort (); \
		break; \
	    } \
        } \
    } while (0)
#else
#define THREAD_SPECIFIC_DATA(type, data, size) \
    do { \
        static type buff[size]; \
        data = buff; \
    } while (0)
#endif /* HAVE_LIBPTHREAD */

#define THREAD_SPECIFIC_STORAGE_LEN 1024
#define THREAD_SPECIFIC_STORAGE_NUM 16
#define THREAD_SPECIFIC_STORAGE2(data, len, num) \
    do { \
        struct buffer { \
            u_char buffs[num][len]; \
            u_int i; \
        } *buffp; \
	\
	THREAD_SPECIFIC_DATA (struct buffer, buffp, 1); \
	if (buffp) { \
	    data = (void*)buffp->buffs[buffp->i++%num]; \
	} \
	else { \
	    data = (void*)NULL; \
	} \
    } while (0)

#define THREAD_SPECIFIC_STORAGE_LEN 1024
#define THREAD_SPECIFIC_STORAGE_NUM 16
#define THREAD_SPECIFIC_STORAGE(data) THREAD_SPECIFIC_STORAGE2 (\
	data, THREAD_SPECIFIC_STORAGE_LEN, THREAD_SPECIFIC_STORAGE_NUM)

#define ASSERT(x) { if (!(x)) \
	err_dump ("\nAssert failed line %d in %s", __LINE__, __FILE__); }

#define NETSHORT_SIZE 2		/* size of our NETSHORT in bytes */
#define NETLONG_SIZE 4		/* size of our NETLONG in bytes */
#define UTIL_GET_NETSHORT(val, cp) \
	{ \
            register u_char *val_p; \
            val_p = (u_char *) &(val); \
            *val_p++ = *(cp)++; \
            *val_p++ = *(cp)++; \
	    (val) = ntohs(val); \
	}

/* This version of UTIL_GET_NETLONG uses ntohl for cross-platform
 * compatibility, which is A Good Thing (tm) */
#define UTIL_GET_NETLONG(val, cp) \
        { \
            register u_char *val_p; \
            val_p = (u_char *) &(val); \
            *val_p++ = *(cp)++; \
            *val_p++ = *(cp)++; \
            *val_p++ = *(cp)++; \
            *val_p++ = *(cp)++; \
            (val) = ntohl(val); \
        }

/* Network version of UTIL_PUT_SHORT, using htons for cross-platform
 * compatibility */
#define UTIL_PUT_NETSHORT(val, cp) \
	{ \
	    u_short tmp; \
            register u_char *val_p; \
	    tmp = htons(val); \
	    val_p = (u_char *) &tmp; \
            *(cp)++ = *val_p++; \
            *(cp)++ = *val_p++; \
	}

/* Network version of UTIL_PUT_LONG which uses htonl to maintain
 * cross-platform byte-orderings across the network */
#define UTIL_PUT_NETLONG(val, cp) \
        { \
            u_long tmp; \
            register u_char *val_p; \
            tmp = htonl(val); \
            val_p = (u_char *) &tmp; \
            *(cp)++ = *val_p++; \
            *(cp)++ = *val_p++; \
            *(cp)++ = *val_p++; \
            *(cp)++ = *val_p++; \
        }


/* public functions */

prefix_t *New_Prefix (int family, void * dest, int bitlen);
prefix_t *Ref_Prefix (prefix_t * prefix);
void Deref_Prefix (prefix_t * prefix);
void print_prefix_list (LINKED_LIST * ll_prefixes);
void print_prefix_list_buffer (LINKED_LIST * ll_prefixes, buffer_t * buffer);
void print_pref_prefix_list_buffer (LINKED_LIST * ll_prefixes, 
				    u_short *pref, buffer_t * buffer);
void print_pref_prefix_list (LINKED_LIST * ll_prefixes, u_short *pref);

prefix_t *name_toprefix(char *, trace_t *);
prefix_t *string_toprefix(char *, trace_t *);

#define prefix_tolong(prefix) (assert ((prefix)->family == AF_INET),\
                               (prefix)->add.sin.s_addr)
#define prefix_tochar(prefix) ((char *)&(prefix)->add.sin)
#define prefix_touchar(prefix) ((u_char *)&(prefix)->add.sin)
#define prefix_toaddr(prefix) (&(prefix)->add.sin)
#define prefix_getfamily(prefix) ((prefix)->family)
#define prefix_getlen(prefix) (((prefix)->bitlen)/8)
#define prefix_toaddr6(prefix) (assert ((prefix)->family == AF_INET6),\
				&(prefix)->add.sin6)

int prefix_compare (prefix_t * p1, prefix_t * p2);
int prefix_equal (prefix_t * p1, prefix_t * p2);
int prefix_compare2 (prefix_t * p1, prefix_t * p2);
int prefix_compare_wolen (prefix_t *p, prefix_t *q);
int prefix_compare_wlen (prefix_t *p, prefix_t *q);
char *prefix_toa (prefix_t * prefix);
char *prefix_toa2 (prefix_t * prefix, char *tmp);
char *prefix_toax (prefix_t * prefix);
char *prefix_toa2x (prefix_t * prefix, char *tmp, int with_len);
prefix_t *ascii2prefix (int family, char *string);
int my_inet_pton (int af, const char *src, void *dst);

int init_mrt (trace_t *tr);
mrt_thread_t *mrt_thread_create (char *name, schedule_t * schedule,
				 thread_fn_t callfn, void *arg);
mrt_thread_t *mrt_thread_create2 (char *name, schedule_t * schedule,
				 thread_fn_t callfn, void *arg);
int is_ipv4_prefix (char *string);
int is_ipv6_prefix (char *string);

int comp_with_mask (void *addr, void *dest, u_int mask);
int byte_compare (void *addr, void *dest, int bits, void *wildcard);

/* Solaris 2.5.1 lacks prototype for inet_ntop and inet_pton */
#if defined(HAVE_DECL_INET_NTOP) && ! HAVE_DECL_INET_NTOP
const char *inet_ntop (int af, const void *src, char *dst, size_t size);
int inet_pton (int af, const char *src, void *dst);
#endif /* HAVE_INET_NTOP */

/* Solaris 2.5.1 lacks prototype for getdtablesize() */
#if defined(HAVE_DECL_GETDTABLESIZE) && ! HAVE_DECL_GETDTABLESIZE
int getdtablesize(void);
#endif

#ifndef HAVE_MEMMOVE
char *memmove (char *dest, const char *src, size_t n);
#endif /* HAVE_MEMMOVE */

#ifndef HAVE_LIBGEN_H
#ifndef HAVE_DIRNAME
char *dirname(char *path);
#endif
#ifndef HAVE_BASENAME
char *basename(char *path);
#endif
#endif /* HAVE_LIBGEN_H */

char *r_inet_ntoa (char *buf, int n, u_char *l, int len);
int prefix_is_loopback (prefix_t *prefix);

int nonblock_connect (trace_t *default_trace, prefix_t *prefix, int port, int sockfd);
void mrt_reboot (void);
int init_mrt_reboot (int argc, char *argv[]);
void mrt_main_loop (void);
void mrt_busy_loop (volatile int *force_exit_flag, int ok);
void mrt_switch_schedule (void);
int mrt_update_pid (void);
void mrt_thread_exit (void);
void mrt_thread_kill_all (void);
void init_mrt_thread_signals (void);
void mrt_exit (int status);
void mrt_process_signal (int sig);
void mrt_set_force_exit (int code);

buffer_t *New_Buffer (int len);
void Delete_Buffer (buffer_t *buffer);

#define LL_Add2(ll, a) LL_Add (((ll)? (ll): ((ll) = LL_Create (0))), (a))
#define LL_Add3(ll, a) { void *_b; \
	LL_Iterate (ll, _b) { if (_b == (a)) break;} if (!_b) LL_Add2 (ll,a);}

#ifndef HAVE_STRTOK_R
#define strtok_r(a,b,c) strtok(a,b)
#endif /* HAVE_STRTOK_R */

#define open(a,b,c) mrt_open((a),(b),(c),__FILE__,__LINE__)
#define close(a) mrt_close((a),__FILE__,__LINE__)
#undef socket
#define socket(a,b,c) mrt_socket((a),(b),(c),__FILE__,__LINE__)
#define accept(a,b,c) mrt_accept((a),(b),(c),__FILE__,__LINE__)

int mrt_open (const char *path, int flags, mode_t mode, char *s, int l);
int mrt_close (int d, char *s, int l);
int mrt_socket (int domain, int type, int protocol, char *s, int l);
int mrt_accept (int d, struct sockaddr *addr, socklen_t *addrlen, char *s, int l);

#define socket_errno()	errno

#endif /* _MRT_H */
