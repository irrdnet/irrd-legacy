#ifndef _IRRD_OPS_H
#define _IRRD_OPS_H

//#include <sys/socket.h>
#include <trace2.h>
#include <irr_notify.h>

/* Do not change the ATTR_ID values */
enum ATTR_ID {
  X_ATTR       = 0,
  NOTIFY_ATTR  = 01, 
  MNT_BY_ATTR  = 02, 
  UPD_TO_ATTR  = 04, 
  MNT_NFY_ATTR = 010,
  MNTNER_ATTR  = 020, 
  AUTH_ATTR    = 040
};

#define is_upd_to(p) (!strncasecmp (p, "upd-to:", 7))
#define is_mnt_nfy(p) (!strncasecmp (p, "mnt-nfy:", 8))
#define is_mnt_by(p) (!strncasecmp (p, "mnt-by:", 7))
#define is_notify(p) (!strncasecmp (p, "notify:", 7))
#define is_mntner(p) (!strncasecmp (p, "mntner:", 7))
#define is_auth(p) (!strncasecmp (p, "auth:", 5))
#define is_changed(p) (!strncasecmp (p, "changed:", 8))
#define is_source(p) (!strncasecmp (p, "source:", 7))

/* Used by read_socket_obj (), SHORT_OBJECT
 * will get auth, upd-to, mnt-nfy, and notify
 * fields only.
 */
#define FULL_OBJECT  0
#define SHORT_OBJECT 1

#define newline_remove(p)  { char *_z = (p) + strlen((p)) - 1; \
	if (*_z == '\n') \
	   *_z = '\0';}

#define SNIP_MSG "[snip... Object abbreviated for readability]\n"

/* function prototypes */
char *irrd_transaction_old (trace_t *, char *, int *, FILE *, char *, char *, 
			    int, int *,	char *, int);
FILE *IRRd_fetch_obj (trace_t *, char *, int *, int *, int, char *o, char *, 
		      char *, int , char *, int);
int close_connection (int);
char *end_irrd_session (trace_t *, int);
int read_socket_line (trace_t *, int, char [], int);

char *irrd_transaction_new (trace_t *, char *, FILE *, ret_info_t *, char *, int);
char *read_socket_cmd (trace_t *, int, char *);
char *send_socket_cmd (trace_t *, int, char *);

/* RPS-DIST support */
char *rpsdist_transaction (trace_t *, char *, char *, char *, char *, char *, 
			   int);
char *irrd_transaction (trace_t *, char *, int *, FILE *, char *, char *, 
			int, int *, char *, int);

/* New routines 7/18/00 */
char *send_rpsdist_trans      (trace_t *, FILE *, char *, int);
char *rpsdist_update_pgp_ring (trace_t *, FILE *, char *);
char *irrd_curr_serial        (trace_t *, char *, char *, int);

#endif /* _IRRD_OPS_H */
