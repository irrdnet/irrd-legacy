/*
 * $Id: irrauth.h,v 1.11 2002/10/17 19:41:44 ljb Exp $
 */


#ifndef _IRR_AUTH_H
#define _IRR_AUTH_H

#include <pipeline_defs.h>
#include <config.h>

#define MAXLEN 2047

/* Max transaction log file size and seperator */
#define MAX_LOG_FILE_SIZE 4096000
#define TRANS_SEPERATOR "\n---\n"

/* macro's */

#define set_op_type(x,y) free_mem (x); x = strdup (y)

/* Used in maint_check ()
 * identify operation type.
 */
#define iDEL 0
#define iADD 1
#define iREPLACE 2

#define pgpargs 	"-o"
#define PUBKEYWORD	"pubring"
#define PUBRINGNM	"pubring.pkr"
#define SECKEYWORD	"secring"
#define SECRINGNM	"secring.skr"


#include <sys/types.h>
#include <regex.h>
#include "hdr_comm.h"
#include "irrd_ops.h"

enum AUTH_CODE {
  AUTH_FAIL_C     = 01, 
  AUTH_PASS_C     = 02, 
  OTHER_FAIL_C    = 04, 
  MNT_NO_EXIST_C  = 010,
  DEL_NO_EXIST_C  = 020, 
  NOOP_C          = 040,
  NEW_MNT_ERROR_C = 0100,
  DEL_MNT_ERROR_C = 0200,
  BAD_OVERRIDE_C  = 0400
};

/* types used in pgpdecodefile () to identify 
 * the signed messaged key type */
enum PGPKEY_TYPE {
  NO_SIG = 0,
  REGULAR_SIG,
  DETACHED_SIG
};

#define CLEAR_NOTIFY (MNT_NO_EXIST_C|DEL_NO_EXIST_C|NOOP_C|OTHER_FAIL_C|AUTH_FAIL_C|NEW_MNT_ERROR_C|DEL_MNT_ERROR_C|BAD_OVERRIDE_C)
#define CLEAR_FORWARD (MNT_NO_EXIST_C|DEL_NO_EXIST_C|NOOP_C|OTHER_FAIL_C|AUTH_PASS_C|NEW_MNT_ERROR_C|DEL_MNT_ERROR_C|BAD_OVERRIDE_C)

extern const char blankline[];
extern const char cookie[];
extern const char cookieins[];
extern const char mailfrom[];
extern const char mailreplyto[];
extern const char mailfromnc[];
extern const char messid[];
extern const char subj[];
extern const char date[];

extern const char pgpbegin[];
extern const char pgpbegdet[];
extern const char pgpend[];
extern const char pgpkeyid[];
extern const char pgpmailid[];
extern const char pgpgood[];
extern const char mntby[];
extern const char origin[];
extern const char dotstar[];
extern const char tmpfntmpl[];
extern const char password[];

extern char auth[16][256];
extern char msgid[];
extern char subjid[];
extern char dateid[];

/* key-cert support */
extern const char key_cert[];
extern const char pubkey_begin[];
extern const char pubkey_end[];

extern trace_t *default_trace;

typedef struct _rxlist {
  const regex_t *re;
  char *buf;
  int flags;
  int counter;
} rxlist;


typedef struct _obj_lookup_t {
  char *key;     /* object key; for route and person both keys */
  char *type;    /* object type, eg, aut-num, mntner, ... */
  char *source;  /* DB source */
  int state;     /* 0 means deleted, 1 means add/replace */
  long fpos;     /* starting file pos */
  FILE *fd;      /* file pointer */
  struct _obj_lookup_t *next;
} obj_lookup_t;

typedef struct _lookup_info_t {
  obj_lookup_t *first;
  obj_lookup_t *last;
} lookup_info_t;

typedef struct _kc_obj_t {
  char *hex_key;
  char *source;
  int  add_op;
  int  syntax_error;
  int  key_match;
  int  sig_decode;
  char *sig_fn;
  FILE *sig_fp;
  struct _kc_obj_t *next;
} kc_obj_t;

typedef struct _kc_info_t {
  kc_obj_t *first;
  kc_obj_t *last;
} kc_info_t;

/* Function prototypes */


int pgpdecodefile (FILE *file, char *, FILE *, char *, trace_t *tr);
int pgpdecodefile_new (FILE *file, char *, FILE *, char *, trace_t *tr);
int addmailcookies (trace_t *, int, char *, char *);
int writecookietofile (char *, char *);
int callsyntaxchk (trace_t *tr, char *, char *, char *);
int callnotify (trace_t *, char *, int, int, int, char *, int, char *, int, 
		char *, char *, char *, char *, long, FILE *, char *);
int auth_check (trace_t *, char *, char *, char *, int, char *);

/* trans_lists.c */
void trans_list_update (trace_t *, lookup_info_t *, trans_info_t *, FILE *, long);
void free_trans_list (trace_t *, lookup_info_t *);
obj_lookup_t *find_trans_object (trace_t *, lookup_info_t *, char *, char *, char *);
void new_kc_obj (trace_t *, kc_info_t *, char *);

/* util.c */
int update_cryptpw (trace_t *, FILE *, long, char *);
int noop_check (trace_t *, FILE *, long, FILE *, long);
char *myconcat (char *, char *);
int find_token (char **, char **);
char *free_mem (char *);
char *filter_duplicates (trace_t *, char *, char *);
void write_trans_obj (trace_t *, FILE *, long, FILE *, int, int);
char *cull_attribute (trace_t *, FILE *, long, u_int);
enum ATTR_ID find_attr (trace_t *, char *, int, u_int, char **);
FILE *myfopen (trace_t *, char *, char *, char *);
void log_roll (char *, char *, int);
void unlock_file (int);
int lock_file (int *, char *filename);
int in_DB (trace_t *, char *, char *, char *, char *, int);
int new_pgpdecodefile (FILE *, char *, FILE *, 
		       char *, char *, int, trace_t *);
int good_signature (trace_t *, char *, kc_obj_t *, char *);
void call_pipeline (trace_t *, FILE *, char *, int, int, int);

#endif /* _IRR_AUTH_H */
