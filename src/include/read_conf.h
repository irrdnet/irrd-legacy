#ifndef _READ_CONF_H
#define _READ_CONF_H

#include <trace2.h>

typedef struct _source_t {
  char *source;         /* source name, eg, RADB, ANS, ... */
  int  authoritative;   /* are we authoritative for this source? */
  char *host;           /* mirroring host (if it is a mirror */
  char *port;           /* mirror port (if it is a mirror */
  /* rpsdist attributes */
  int  rpsdist_flag;
  int  rpsdist_auth;
  int  rpsdist_trusted;
  char *rpsdist_host;
  char *rpsdist_port;
  char *rpsdist_pgppass;
  struct _source_t *next;
} source_t;

typedef struct _src_t {
  source_t *first;
  source_t *last;
} src_t;

typedef struct acl_t{                                 /* Hold info about ACL's */
  char         * host;                                /* ALLOW address */
  char         * db_name;                             /* name of DB (if any) */
  struct acl_t * next;                                /* next pointer */
} acl;

typedef struct _config_info_t {
  int      irrd_port;     /* IRRd port */
  char     *irrd_host;    /* IRRd host */
  char     *super_passwd; /* super user/override password */
  char     *log_dir;      /* path to the acklog */
  char     *db_dir;       /* path to the database dir */
  char     *pgp_dir;      /* pgp dir */
  char     *db_admin;     /* db admin email addr */
  char     *footer_msg;   /* submission footer msg's */
  char     *notify_header_msg;   /* header on submissions msgs */ 
  char     *forward_header_msg;   
  int	   allow_inetdom;  /* flag to allow inetnum/inet6num/as-block/domain objs */
  source_t *srcs;         /* pointer to the source_t list */
  acl      *acls;         /* accept ACL list */
} config_info_t;

/* function prototypes */

int      parse_irrd_conf_file (char *, trace_t *tr);
void     add_src_obj          (src_t *, source_t *);
source_t *create_source_obj   (char *, int);
int      config_trace_local   (trace_t *tr, char *line);
char     *dir_chks            (char *, int);

#endif /* _READ_CONF_H */
