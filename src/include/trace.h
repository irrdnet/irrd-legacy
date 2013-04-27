/* 
 * $Id: trace.h,v 1.2 2002/10/17 19:41:44 ljb Exp $
 */

#ifndef _TRACE_H
#define _TRACE_H

#include <sys/types.h>
#include <signal.h>
#include <linked_list.h>
#include <buffer.h>

#include <mrt_thread.h>

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

/* logging/error conditions */
#define TR_FATAL	0x0001	/* fatal error -- die after receiving */
#define TR_ERROR	0x0002	/* non fatal error -- continuing */
#define TR_WARN		0x0004	/* warning -- inform to syslog */

#define TR_INFO		0x0008	/* normal events */
#define TR_TRACE	0x0010	/* verbose tracing, like keepalives */

#define FATAL		TR_FATAL	/* backward compatibility */
#define ERROR		TR_ERROR	/* backward compatibility */
#define INFO		TR_INFO	/* backward compatibility */
#define NORM		TR_INFO	/* backward compatibility */
#define TRACE		TR_TRACE	/* backward compatibility */

#define TR_PARSE	0x0020	/* trace parsing of config files */
#define TR_PACKET	0x0040	/* trace packet comming and goings */
#define TR_STATE	0x0080	/* trace state changes and events */
#define TR_TIMER	0x0100	/* trace timer changes and events */
#define TR_POLICY	0x0200	/* trace policy changes and events */

#define TR_THREAD	0x0400	/* thread tracing */
#define TR_DEBUG	0x0800	/* debugging */

#define TR_ALL		0xffff

/* Type of log flags */
#define TR_LOG_FILE	0x1
#define TR_LOG_SYSLOG	0x2

#define TR_DEFAULT_FLAGS	(TR_FATAL|TR_ERROR|TR_WARN)
#define TR_DEFAULT_APPEND	TRUE
#define TR_DEFAULT_FLOCK	TRUE
/* We do not currently support LARGE files, so limit filesize to max int */
#define TR_DEFAULT_MAX_FILESIZE	2147483647	/* Max size for 32 bit files */
#define TR_DEFAULT_SYSLOG	TR_LOG_FILE	/* Use logfile only */

typedef struct _error_list_t {
    LINKED_LIST *ll_errors;
    int max_errors;
    pthread_mutex_t mutex_lock;
} error_list_t;

#define DEFAULT_MAX_ERRORS      20

/* The main trace structure. A "slave" trace is a copy of the "master"
 * trace. Slaves use the master's output fd, filename and mutex lock.
 * This way we can do some minor customization, like prepending and set
 * flags individually ala GateD for various MRT modules (like BGP peers).
 */

/* Now, the shared part was isolated and defined separately */
typedef struct _logfile_t
{
    char *logfile_name;
    char *prev_logfile;		/* Previously open log file */
    FILE *logfd;
    int ref_count;		/* reference counter */
    pthread_mutex_t mutex_lock;
    pthread_t thread_id;	/* a thread can have trace "open" for 
				   multi-line tracing */
    u_char append_flag;		/* if 1, append to log file */
    u_char flock_flag;		/* if 1, use flock to access trace file */
    u_int  max_filesize;	/* Max. filesize of trace, in bytes */

    u_long logsize;
    u_long bytes_since_open;    /* we periodically reopen, and append to file
				 * after writing xxx number of bytes. This is
				 * so if don't /dev/null log data if file removed,
				 * but we still have inode
				 */
} logfile_t;

typedef struct _trace_t {
    int	 slave;			/* are we just a copy of a master trace? */ 
    struct _uii_connection_t *uii;	/* if tracing has been redirected to 
				   a terminal session */
    int	(*uii_fn)();	/* function for uii (not to link it) */
    char *prepend;		/* prepend the string */
    error_list_t *error_list;	/* store error messages */
    int flags;
    u_char syslog_open;		/* Flag:  TRUE if openlog() has been called,
					FALSE otherwise */
    u_char syslog_flag;		/* 1 = use logfile
				 * 2 = use syslog
				 * 3 = use both */
    logfile_t *logfile;
    buffer_t *buffer;		/* internal use */
} trace_t;

enum Trace_Attr {
    TRACE_LOGFILE = 1,
    TRACE_FLAGS,
    TRACE_FLOCK,		/* use flock when accessing trace file */
    TRACE_APPEND,		/* append to log file */
    TRACE_ADD_FLAGS,
    TRACE_DEL_FLAGS,
    TRACE_USE_SYSLOG,
    TRACE_MAX_FILESIZE,
    TRACE_PREPEND_STRING,
    TRACE_MAX_ERRORS
};

/* public functions */
trace_t *New_Trace (void);
trace_t *New_Trace2 (char *);
int set_trace (trace_t * tr, int first,...);
int trace (int severity, trace_t * tr,...);
int trace_open (trace_t * tr);
int trace_close (trace_t * tr);
trace_t *trace_copy (trace_t * tr);
void Destroy_Trace (trace_t * tr);
u_int trace_flag (char *str);
int init_trace(const char *, int);
void trace_recover_signal (sigset_t old);
#ifndef HAVE_STRERROR
char *strerror (int errnum);
#endif /* HAVE_STRERROR */
int set_trace_global (trace_t *tmp);
int okay_trace (trace_t *tr, int flags);

#endif /* _TRACE_H */
