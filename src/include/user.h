/*
 * $Id: user.h,v 1.1.1.1 2000/02/29 22:28:43 labovit Exp $
 */

#ifndef _USER_H
#define _USER_H

#include <New.h>
#include <linked_list.h>
#include <timer.h>

#define UII_DEFAULT_PORT 5674

#define UII_OUTPUT_BUFFER_INIT_SIZE 256

#define UII_UNPREV 	         0
#define UII_INPUT 	         1
#define UII_NORMAL               2
#define UII_ENABLE               3
#define UII_CONFIG               4
#define UII_CONFIG_ROUTER        5	/* obsolete */
#define UII_CONFIG_ROUTE_MAP     6
#define UII_CONFIG_INTERFACE     7
#define UII_CONFIG_NETWORK_LIST  8	/* used in bgpsim */
#define UII_CONFIG_ROUTER_RIP    9
#define UII_CONFIG_ROUTER_RIPNG 10
#define UII_CONFIG_ROUTER_BGP   11
#define UII_CONFIG_ROUTER_OSPF  12
#define UII_CONFIG_LINE	        13
#define UII_CONFIG_ROUTER_DVMRP 14
#define UII_CONFIG_ROUTER_PIM	15

#define UII_PROMPT_MORE         20	/* --more-- */
#define UII_MAX_STATE	        21	/* the last plus one */
#define UII_MAX_LEVEL	         8  /* exit, unprev, normal, enable, config (2) */

#define UII_ALL	       	        98	/* match with all levels except 0 */
#define UII_CONFIG_ALL	        99	/* match with all levels 
					   more than UII_CONFIG */
#define UII_EXIT	     1000	/* user connection has terminated */

#define COMMAND_NORM		0
#define COMMAND_NODISPLAY	1
#define COMMAND_DISPLAY_ONLY	2
#define COMMAND_MATCH_FIRST	4	/* for password and comments -- 
					     match if any of command matches */
					 
/* options to set in uii */
enum UII_ATTR {
    UII_NULL = 0,
    UII_PROMPT,			/* the default prompt */
    UII_ACCESS_LIST,		/* Access list number */
    UII_PASSWORD,		/* Password for telnet access */
    UII_PORT,			/* Port to listen on */
    UII_ADDR,			/* Address to listen at */
    UII_INITIAL_STATE,		/* what state do we start in */
    UII_ENABLE_PASSWORD		/* Password for enable */
};

#define UII_DEFAULT_TIMEOUT 300	/* in sec */

typedef struct _uii_t {
    trace_t *trace;
    int sockfd;			/* socket for connectoin requests */
    int port; 			/* port to listen on */ 
    				/* UII telnet access is disabled if port < 0 */
    int default_port; 		/* default port number */ 
    int timeout;		/* uii idle timeout (sec) */
    prefix_t *prefix;		/* address to listen at */
    LINKED_LIST *ll_uii_commands;
    LINKED_LIST *ll_uii_connections;
    char *prompts[UII_MAX_STATE];
    int initial_state;		/* obsolete, initial_state is decided 
					by existance of passowrd */
    int login_enabled;          /* UII telnet access is enabled */
    char *service_name;		/* what we are called in /etc/services */
    int access_list;		/* access list to check before accepting telnets */
    char	*password;	/* password "securing" the telnet connection */
    char	*enable_password;	/* password for enable mode */
    char	*redirect;	/* directory able to redirect */
    char *hostname;
    pthread_mutex_t     mutex_lock;
    schedule_t *schedule;	/* for retrying uii open */
    mtimer_t   *timer;		/* for retrying uii open */
} uii_t;

typedef struct _uii_connection_t {
    int state;
    int prev_level;
    int previous[UII_MAX_LEVEL];
    int sockfd;
    buffer_t *buffer;
    int pos;			/* current cursor position in line */
    int end;			/* end of line position */
    char *cp;			/* for easy access to the buffer */
    LINKED_LIST *ll_history;	/* command history */
    char *history_p;		/* current pointer to history list */
    int negative;		
    char *comment;		/* pointer to comment */
  /*  int show;		 the command is being called from show config */ 
    int protocol;		/* will be obsolete */

    LINKED_LIST *ll_tokens;

    /* used when a uii requests gobs of data from another thread 
     * that thread fills in a buffer and signals the waiting thread
     */
#ifdef HAVE_LIBPTHREAD
    pthread_cond_t      cond;
    pthread_mutex_t     mutex_cond_lock;
#endif /* HAVE_LIBPTHREAD */
    buffer_t *answer;
    int	tty_current_line;	/* current line of the tty */
    int tty_max_lines;		/* max number of lines on telnet tty */

    schedule_t *schedule;
    prefix_t *from;
    int sockfd_out;		/* used for console output and  redirection */

    /* telnet negotiation stuff */
    int telnet;
    u_char telnet_buf[15];

    pthread_mutex_t       *mutex_lock_p; /* grab a lock for config mode */
    char *more;		/* resume point */
    int retries;		/* password retries */
#ifndef HAVE_LIBPTHREAD
    mtimer_t *timer;		/* idle timeout timer */
#endif /* HAVE_LIBPTHREAD */
    int		data_ready;	/* uii input */
    int control;
} uii_connection_t;

/* typedef int (*uii_call_fn_t)(uii_connection_t *, ...); */
typedef int (*uii_call_fn_t)();

/* Each object submits a list of commands that is will respond to */
typedef struct _uii_command_t {
    char *string;
    uii_call_fn_t call_fn;
    int state;			/* UII_UNPREV, UII_NORMAL, UII_CONFIG, etc */
    int flag;			/* COMMAND_NORM, COMMAND_NODISPLAY, etc */
    char *explanation;		/* helpful text on what this command does */
    LINKED_LIST *ll_tokens;	/* instead of a string, list of tokens to match on */
    schedule_t *schedule;	/* schedule it if expected so */
    void *arg;                  /* passed back to call_fn */
	 int usearg;						/* does call_fn expect arg? */
} uii_command_t;


extern uii_t *UII;

int uii_add_command (int state, char *cmd, uii_call_fn_t call_fn);
int uii_add_command2 (int state, int flag, char *string, 
		      uii_call_fn_t call_fn, char *explanation);
int uii_add_command_arg (int state, int flag, char *string,
		      uii_call_fn_t call_fn, void *arg,
		      char *explanation);
int uii_add_command_schedule (int state, int flag, char *string,
      uii_call_fn_t call_fn, char *explanation, schedule_t *schedule);
int uii_add_command_schedule_arg (int state, int flag, char *string,
      uii_call_fn_t call_fn, void *arg, char *explanation,
      schedule_t *schedule);
int uii_process_command (uii_connection_t * uii);
uii_t *New_UII (trace_t * ltrace);
int uii_send_data (uii_connection_t * uii, char *format, ...);
int uii_send_buffer (uii_connection_t * uii, buffer_t *buffer);
int uii_send_buffer_del (uii_connection_t * uii, buffer_t *buffer);
void init_uii (trace_t * trace);
void init_uii_port (char *portname);
int listen_uii (void);
int listen_uii2 (char *port);
char *uii_parse_line (char **line);
char *uii_parse_line2 (char **line, char *word);
int parse_line (char *line, char *format, ...);
int set_uii (uii_t *, int, ...);
int uii_check_passwd (uii_connection_t *uii);
char *strip_spaces (char *tmp);
int uii_show_timers (uii_connection_t *uii);
void signal_uii_data_ready (uii_connection_t *uii);
uii_command_t *uii_match_command (uii_connection_t *uii);
int uii_show_help (uii_connection_t *uii);
int uii_add_bulk_output (uii_connection_t *uii, char *format, ...);
int uii_add_bulk_output_raw (uii_connection_t *uii, char *string);
void uii_send_bulk_data (uii_connection_t * uii);
void uii_command_tab_complete (uii_connection_t *uii, int alone);
int uii_destroy_connection (uii_connection_t * connection);
void user_notice (int trace_flag, trace_t *tr, uii_connection_t *uii,
             char *format, ...);
void user_vnotice (int trace_flag, trace_t *tr, uii_connection_t *uii, 
	     char *format, va_list args);
void print_error_list (struct _uii_connection_t *uii, trace_t *tr);
int uii_yes_no (uii_connection_t *uii);
int uii_read_command (uii_connection_t * uii);
int uii_token_match (char *format, char *command);
LINKED_LIST *
find_matching_commands (int state, LINKED_LIST * ll_tokens,
			LINKED_LIST * ll_last, int alone);
LINKED_LIST *uii_tokenize (char *buffer, int len);
LINKED_LIST *uii_tokenize_choices (char *buffer, int len);
int 
uii_add_command_schedule_i (int state, int flag, char *string, uii_call_fn_t call_fn,
	   void *arg, int usearg, char *explanation, schedule_t * schedule);

#endif /* _USER_H */
