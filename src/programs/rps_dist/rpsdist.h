#ifndef _RPS_H_
#define _RPS_H_

#include <config.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <trace.h>
#include <mrt_thread.h>
#include <regex.h>
#include <read_conf.h>                    /* read_irrd_conf file */
#include <hdr_comm.h>                     /* for MAXLINE */

#define IRR_FILE_LIFETIME   1800          /* 30 minutes, delete files older than this */
#define POLL_INTERVAL       900           /* 15 minutes, transaction request thread wakes up atleast this often */

#define DEBUG               0
#define EXIT_ERROR          1       
#define MAX_HELD_TRANS      10            /* max number of transactions to hold PER DB */
#define FREE_MTRANS_NODE    1             /* when freeing a mirror transaction, free the first node? */
#define NO_FREE_MTRANS_NODE 0

enum ENCODING_T{
  PLAIN = 0,
  GZIP
};

enum INTEGRITY_T{
  LEGACY,
  NO_AUTH,
  AUTH_FAIL,
  AUTH_PASS,
  NONE = 0         /* as in 'none specified' */
};

typedef struct{                                       /* holds the info from a "transaction-begin:" object */
  long                 length;                        /* how long the original file should be */
  enum ENCODING_T      encoding;                      /* how it's encoded */  
  char               * pln_fname;                     /* after decoding (or if not needed), plaintext is here */
} trans_begin_h;

typedef struct{                                       /* hold the info from a "transaction-label" object */
  char *               label;                         /* label, as in name of repository */
  long                 serial;                        /* serial or sequence number */
  long                 timestamp;                     /* timestamp from the sender */
  enum INTEGRITY_T     integrity;                     /* integrity from the sender */
} trans_label_h;

typedef struct auth_dep_t{                            /* holds auth dependency objects */
  char *               name;                          /* name of the dependency, a repository */
  long                 serial;                        /* serial number it's dependent on of that repos */
  long                 timestamp;                     /* timestamp */
  struct auth_dep_t *  next;                          /* pointers to more dependencies */
} auth_dep_h;

typedef struct rep_sig_t{                             /* holds the repository signature objects */
  char *               repository;                    /* name of the signing repository */
  long                 fpos_start;                    /* where in the file this rep. sig. starts */
  long                 fpos_end;                      /* where it ends */
  enum INTEGRITY_T     integrity;                     /* how the signing repository classified the transaction */
  struct rep_sig_t *   next;                          /* more signatures */
} rep_sig_h;

struct mt_t;                                          /* forward declaration */

typedef struct _rps_database_t {                      /* holds all our information about a given DB */
  char                     * name;                    /* the name of the DB */
  long                       first_serial;            /* the first serial available for request */
  long                       current_serial;          /* the current and last serial */
  u_short                    authoritative;           /* if we are authoritative for the DB or not */
  long                       crash_serial;            /* used during bootstrap, holds the serial of a crashed transaction */
  FILE                     * journal;                 /* journal file pointer */
  long                       jsize;                   /* used during bootstrap to hold the size of the journal file */
  struct mt_t              * held_transactions;       /* linked list of currently held transactions */
  pthread_mutex_t            lock;                    /* lock to change data inside this struct */
  u_short                    trusted;                 /* is this a trusted DB? */
  char                     * hexid;                   /* HexId we use to sign */
  char                     * pgppass;                 /* PGP password for the hex id */
  unsigned long              host;                    /* mirror host (if any - network order) */
  int                        port;                    /* mirror port (if any) */
  time_t                     time_last_recv;          /* last time we recieved something from this DB */
  struct _rps_database_t   * next, * prev;            /* next and prev database */
}rps_database_t;

typedef struct _rps_connection_t{                     /* hold connection information for a connection */    
  int                   sockfd;                       /* the actual socket */
  u_long                timeout;                      /* seconds before idle connection times out*/
  char                  buffer[MAXLINE];              /* receive buffer */
  char                  tmp[MAXLINE];                 /* used to store entire lines gathered form the buffer */
  char                * cp;                           /* pointer to cursor in line */
  char                * end;                          /* pointer to end of line */
  unsigned long         host;                         /* remote host address (network order) */
  char                * db_name;                      /* name of remote DB */
  pthread_mutex_t       write;                        /* lock to get before writing to sockfd */
  time_t                time_last_sent;               /* last time we SENT something down this socket */
  struct _rps_connection_t *next, *prev;              /* next and prev */
} rps_connection_t;

typedef struct mt_t{                                  /* holds information about a mirror transaction as we parse it */
  trans_begin_h      trans_begin;                     /* the transaction header info */
  trans_label_h      trans_label;                     /* the transaction label */
  auth_dep_h         auth_dep;                        /* any auth dependencies */
  long               timestamp;                       /* the timestamp */
  int                has_pgp_objs;                    /* whether the transaction has any PGP updates */
  char             * db_pgp_xxx_name;                 /* name of the file for pgp updates */
  rep_sig_h          rep_sig;                         /* list of repository signatures */
  unsigned long      from_host;                       /* the host that sent the transaction */
  struct mt_t *   next;                               /* the next mirror transaction: used in held transactions */
}mirror_transaction;

struct select_master_t{                               /* root datastructure for a set of multiplexed sockets */
  pthread_mutex_t mutex_lock;                         /* lock for this struct */
  int interrupt_fd[2];                                /* when new descriptors need to be added, data is signalled here */
  rps_connection_t * active_links;                    /* linked list of active connections */
  int max_fd_value;                                   /* used to speed-up select, the max FD passed into select */
  fd_set read_set;                                    /* the select read set */
  void * call_func;                                   /* when a socket has new data to be processed, call this function */
};

typedef struct{                                       /* hold information about a user update */
  char * us_name;                                     /* name of the update file ready for IRRd */
  char * pgp_name;                                    /* name of the file with PGP entries */
  char * upd_name;                                    /* the original file name */
} irr_submit_info_t;

typedef struct rpsdist_acl_t{                         /* Hold info about ACL's */
  unsigned long addr;                                 /* ALLOW address */
  char * db_name;                                     /* Which db this corresponds to */
  struct rpsdist_acl_t * next;                        /* next pointer */
} rpsdist_acl;

extern rps_database_t * db_list;                      /* list of known databases */
extern config_info_t ci;                              /* configuration info */
extern rpsdist_acl * mirror_acl_list;                 /* ACL's */

/*
 * Regexes for various RPS DIST structs
 */
regex_t /*trans_sub_begin, resp_auth_type, trans_conf_type, trans_sub_end, */trans_label;
regex_t sequence, timestamp, integrity, auth_dep, rep_sig, signature, trans_begin;
regex_t trans_method, trans_request, seq_begin, seq_end, trans_resp, heartbeat;
regex_t blank_line, key_cert, add_peer, rm_peer, sh_peer;

/* compression_utils */
void zip_file(char * inflnm);
void unzip_file(char * inflnm);

/* file_utils */
char * fcopy(char * srcnm, char * dstnm);
int remove_db_crash_files(char * db_name, long serial);
int remove_db_pgp_copies(char * db_name);
void remove_irr_submit_files();
FILE * open_db_journal(char * db_name, char * mode);
FILE * open_db_trans_xxx(char * db_name, char * mode, long serial);
FILE * open_db_xxx(char * db_name, char * mode, long serial);
FILE * open_db_jentry_xxx(char * db_name, char * mode, long serial);
FILE * open_db_pgp_xxx(char *db_name, char * mode, long serial);
char * append_file_to_file(FILE * dest, char * fname, char * start_str);
int dump_pgp_block(FILE * src, char * dest_file);
void remove_update_files();
char * dump_keycert_from_db_to_file(char * db_name, char * cert, char * irrd_host, int irrd_port);

/* init_utils */
int init_databases(void);
void init_db_pgp_dir(char * db_name);
void init_rps_regexes(void);
int db_crash(rps_database_t * db);
int db_trans_file_intact(rps_database_t * db, long * start, char * pgp_file, long * jsize, long * end);
int handle_db_crash(rps_database_t * db);
void parse_ci_srcs();
void parse_ci_acls();
void check_directories();
void add_missing_dbs();
void add_db(rps_database_t * new_db);
void check_auth_db(rps_database_t * new_db);

/* journal */
void serial_from_journal(rps_database_t * db);
void start_new_journal(rps_database_t * db, long start);
long find_eof(FILE * fp);
int journal_write_end_serial(rps_database_t * db, long serial);
int journal_write_start_serial(rps_database_t * db, long serial);
int journal_write_eof(rps_database_t * db);
char * append_jentry_to_db_journal(rps_database_t * db, char * jname);

/* select */
rps_connection_t * rps_select_add_peer(int fd, struct select_master_t *);
void rps_select_remove_peer(int fd, struct select_master_t *);
void rps_select_loop(struct select_master_t *);
void rps_select_init(void *, struct select_master_t *);
rps_connection_t * rps_select_find_fd(int fd, struct select_master_t *);
rps_connection_t * rps_select_find_fd_by_host(unsigned long host, struct select_master_t *);

/* update */
char *  build_db_trans_xxx_file(rps_database_t * db, char * pgp_name);
char * copy_pgp_rings(rps_database_t * db);
void restore_pgp_rings(rps_database_t * db);
char * send_db_xxx_to_irrd(rps_database_t * db);
char * forward_file_to_peers(char * flnm, struct select_master_t * sm, unsigned long skip_host);
char * append_rep_sig(FILE * dest, char * sig_file_name);
int send_trans_begin(rps_connection_t * irr, char * flnm);
int write_trans_begin(FILE * fp, char * flnm);

/* user_submit */
int handle_user_submit(rps_connection_t * irr);
int user_cleanup(rps_connection_t * irr, rps_database_t * db, char * ret_code);
char * read_irr_submit_info(rps_connection_t * irr, irr_submit_info_t * irr_info);
char * rename_irr_submit_files(irr_submit_info_t * irr_info, rps_database_t * db);
int send_irr_submit_buffer(int sockfd, char * buf);
char * process_jentry_file(rps_database_t * db);

/* mirror_update */
int handle_mirror_flood(mirror_transaction * mtrans);
void free_mirror_trans(mirror_transaction * mtrans, int free_this);
void free_auth_dep_list(auth_dep_h * adp);
void free_rep_sig_list(rep_sig_h * rsp);
int hold_transaction(rps_database_t * db, mirror_transaction * mtrans);
mirror_transaction * mtdupe(mirror_transaction * mtrans);
void process_held_transactions(rps_database_t * db);
int strip_trans_begin(rps_connection_t * irr, mirror_transaction * mtrans);
int parse_trans_label(FILE * fp, mirror_transaction * mtrans);
int check_trans_label(mirror_transaction * mtrans);
int parse_auth_dep(FILE * fp, mirror_transaction * mtrans);
int parse_objects(FILE * fp, mirror_transaction * mtrans);
int parse_timestamp(FILE * fp, mirror_transaction * mtrans);
int parse_user_sigs(FILE * fp);
int parse_rep_sigs(FILE * fp, mirror_transaction * mtrans);
int strip_heartbeat(rps_connection_t * irr);
int strip_trans_request(rps_connection_t * irr);
int integrity_strtoi(char * integ);
int process_db_queues(void);
int send_trans_request(rps_connection_t * irr, char * db_name, long start_serial, long end_serial);
int send_mirror_buffer(rps_connection_t * irr, char * buf, int length);
int from_trusted_repository(mirror_transaction * mtrans);
int process_mirror_transaction(mirror_transaction * mtrans);
char * sign_jentry(rps_database_t * db, char * jentry_nm);
int check_rep_sig(char * rep, mirror_transaction * mtrans);
int process_keycert(FILE * fp, char * hexid, mirror_transaction * mtrans);
char * rpsdist_auth(rps_database_t *, char * jentry_name);
int currently_holding(rps_database_t * db,  long serial);
int already_connected_to(unsigned long host);
void process_trans_request(rps_connection_t * irr, char * db_name, long first, long last);
char * get_new_source_keycert(char * new_src, unsigned long from_host, char * reseed_addr, char * hexid);
int _check_rep_sig(char * rep, char * pgppath, mirror_transaction * mtrans);
int add_new_db(char * new_name, char * new_hexid, char * get_from_addr);
char * find_host_db(unsigned long host);
int check_mirror_connections();

/* utils */
void flush_connection(rps_connection_t * irr);
int sockn_to_file(rps_connection_t * irr, char ** filename, int length);
void trans_rollback(rps_database_t * db);
long tstol (char * timestamp);
char * time_stamp(char * buf, int blen);
rps_connection_t * find_peer_obj (rps_connection_t * list, char * host, int port);
rps_connection_t * create_peer_obj (int fd, char * host, int port);
void add_peer_obj (rps_connection_t * list, rps_connection_t *obj);
void delete_peer_obj(rps_connection_t * list, rps_connection_t * obj);
rps_database_t * find_database(char * name);
void newline_to_null(char * buf);
int read_buffered_line(trace_t * default_trace, rps_connection_t * irr);
int read_buffered_amt(trace_t * default_trace, rps_connection_t * irr, int length);
int safe_write(int sock, char * buf, int length);
int filen_to_sock(rps_connection_t * irr, char * filename, int size);
int _filen_to_sock(rps_connection_t * irr, FILE * fp, int size);
int add_acl(char * host, char * db_name, rpsdist_acl ** acl_list);
rpsdist_acl * find_acl_long(unsigned long addr, rpsdist_acl ** acl_list);
rpsdist_acl * find_acl_char(char * host, rpsdist_acl ** acl_list);
int remove_acl_char(char * host, rpsdist_acl ** acl_list);
int remove_acl_long(unsigned long addr, rpsdist_acl ** acl_list);
unsigned long name_to_addr(char * name);
char * get_db_line_from_irrd_obj(char * obj_src, char * obj_key, char * host, 
				 int port, char * obj_type, char * line_regex);
int synch_databases(rps_connection_t * irr, rps_database_t * db);


#endif
