/*
 * $Id: config_file.h,v 1.2 2001/07/13 18:10:10 ljb Exp $
 */

#ifndef _LCONFIG_H
#define _LCONFIG_H

#include <mrt.h>

#ifndef MAXHOSTNAMELEN
#define MAXHOSTNAMELEN 256
#endif


typedef struct _config_module_t {
  char *name;
  void_fn_t call_fn;
  void *arg;
  u_long flags;		/* CF_WAIT, CF_DELIM, etc */
} config_module_t;


#define CF_WAIT		0x01	/* need to wait for thread to signal us data is ready */
#define CF_DELIM	0x02	/* we' somethings like router_bgp and need a ! delimiter */
#define CF_RAW		0x04    /* special control characters -- no interpretae */

typedef struct _config_t {
  char			*filename;
  int			line;		/* when parsing file, current lineno */
  trace_t		*trace;
  LINKED_LIST		*ll_modules;	/* linked list of config modules */
  pthread_mutex_t	ll_modules_mutex_lock;
  pthread_cond_t	cond;
  pthread_mutex_t	mutex_cond_lock;
  int			ignore_errors;	/* flag if we should exit after encountering
					 * confi file error */
  buffer_t		*answer;
  pthread_mutex_t	mutex_lock;	/* only one can enter config mode */
  int timeout_min;       /* exec timeout value (min) */
  int timeout_sec;       /* exec timeout value (sec) */
  int port;	         /* uii port number */
  int show_password;	/* show clear text password */
  int reading_from_file;
  int state_eof;	/* this is a flag to enable old "!" state EOF */
} config_t;

typedef struct _config_command_t {
  char *name;
  int_fn_t call_fn;
  char *format;
  char *description;
} config_command_t;

int config_from_file (trace_t *trace, char *filename);
int config_from_file2 (trace_t *trace, char *filename);
int config_add_module (int wait_flag, char *name, void_fn_t call_fn, void *arg);
int config_del_module (int flags, char *name, void_fn_t call_fn, void *arg);
void config_notice (int flag, uii_connection_t *uii, char *format, ...);
int start_config (uii_connection_t *uii);
void end_config (uii_connection_t *uii);
void config_add_output (char *format, ...);
int init_config (trace_t *tr);
int get_alist_options (char *options, prefix_t ** wildcard, int *refine, 
		       int *exact);

/* these two functions are reffered from datadist */
void get_comment_config (char *comment);
void get_debug_config (void);

/* from tpd */
int show_config (uii_connection_t * uii);
int config_write (uii_connection_t * uii);

extern config_t CONFIG;

#endif /* _LCONFIG_H */
