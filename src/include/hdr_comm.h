/*
 * $Id: hdr_comm.h,v 1.9 2002/10/17 19:41:44 ljb Exp $
 */


#ifndef _HDR_COMM_H
#define _HDR_COMM_H

#include <stdio.h>
#include <sys/types.h>
#undef close
#undef open
#undef socket
#include <read_conf.h>

#define newline_remove(p)  { char *_z = (p) + strlen((p)) - 1; \
        if (*_z == '\n') \
           *_z = '\0';}

/* max size of any object line */
#define MAXLINE 4096

/* The maximum number of lines for IRRd DB 
 * object retrieval.  This will limit the size
 * of objects returned to the user in email.
 */
#define MAX_IRRD_OBJ_SIZE 350

/* Transaction types */
#define ADD_OP     "ADD"
#define DEL_OP     "DEL"
#define REPLACE_OP "REPLACE"
#define NOOP_OP    "NOOP"
/*
#define NOT_SET_OP "NOT_SET"
*/
/* give a name to the non-email user */
#define TCP_USER   "TCP"

typedef struct _trans_info_t {
  int trans_success;
  char sender_addrs[MAXLINE];
  char *snext;
  char *subject;
  char *date;
  char *msg_id;
  char *obj_type;
  char *obj_key;
  char *source;
  char *pgp_key;
  char *override;
  char *keycertfn;
  char *web_origin_str;
  int  syntax_errors;
  int  syntax_warns;
  int  del_no_exist;
  int  authfail;
  char *otherfail;
  int  new_mnt_error;
  int  del_mnt_error;
  int  bad_override;
  int  unknown_user;
  char *check_notify;
  char *check_mnt_nfy;
  /* new ones, need to resolve later */
  char *notify;
  char *forward;
  char *maint_no_exist;
  char *crypt_pw;
  /* ------ */
  char mnt_by[MAXLINE];
  char notify_addrs[MAXLINE];
  char *nnext;
  char *notify_list;
  char forward_addrs[MAXLINE];
  char *fnext;
  char *forward_list;
  char *op;
  char *old_obj_fname;
  u_int hdr_fields;
} trans_info_t;

#define NOTIFY_REQUIRED_FLDS (FROM_F|DATE_F|SUBJECT_F|MSG_ID_F|HDR_END_F)
#define ERROR_FLDS (SYNTAX_ERRORS_F|DEL_NO_EXIST_F|OTHERFAIL_F|AUTHFAIL_F|MAINT_NO_EXIST_F|NEW_MNT_ERROR_F|DEL_MNT_ERROR_F|BAD_OVERRIDE_F|UNKNOWN_USER_F)
enum HDR_FLDS_T {
  FROM_F           = 01,
  SOURCE_F         = 02,
  SYNTAX_ERRORS_F  = 04,
  DEL_NO_EXIST_F   = 010,
  AUTHFAIL_F       = 020,
  MAINT_NO_EXIST_F = 040,
  OP_F             = 0100,
  NOTIFY_F         = 0200,
  MNT_NFY_F        = 0400,
  UPD_TO_F         = 01000,
  OLD_OBJ_FILE_F   = 02000,
  OBJ_TYPE_F       = 04000,
  OBJ_KEY_F        = 010000,
  SUBJECT_F        = 020000,
  DATE_F           = 040000,
  MSG_ID_F         = 0100000,
  HDR_END_F        = 0200000,
  PGP_KEY_F        = 0400000,
  SYNTAX_WARNS_F   = 01000000,
  OVERRIDE_F       = 02000000,
  MNT_BY_F         = 04000000,
  OTHERFAIL_F      = 010000000,
  CHECK_NOTIFY_F   = 020000000,
  CHECK_MNT_NFY_F  = 040000000,
  PASSWORD_F       = 0100000000,
  KEYCERTFN_F      = 0200000000,
  NEW_MNT_ERROR_F  = 0400000000,
  DEL_MNT_ERROR_F  = 01000000000,
  BAD_OVERRIDE_F   = 02000000000,
  UNKNOWN_USER_F   = 04000000000
};


/* transaction file header strings */

/* hdr info from irr_auth */
extern const char DEL_NO_EXIST[];
extern const char AUTHFAIL[];
extern const char MAINT_NO_EXIST[];
extern const char NOTIFY[];
extern const char MNT_NFY[];
extern const char UPD_TO[];
extern const char OLD_OBJ_FILE[];
extern const char OTHERFAIL[];
extern const char NEW_MNT_ERROR[];
extern const char DEL_MNT_ERROR[];
extern const char BAD_OVERRIDE[];

/* hdr info from irr_check */
extern const char HDR_START[];
extern const char HDR_END[];
extern const char OBJ_TYPE[];
extern const char OBJ_KEY[];
extern const char OP[];
extern const char SOURCE[];
extern const char SYNTAX_ERRORS[];
extern const char SYNTAX_WARNS[];
extern const char MNT_BY[];
extern const char OVERRIDE[];
extern const char KEYCERTFN[];
extern const char SYNTAX_WARNS[];
extern const char CHECK_NOTIFY[];
extern const char CHECK_MNT_NFY[];

/* hdr info from pgp and email processing */

extern const char PGP_KEY[];
extern const char FROM[];
extern const char DATE[];
extern const char SUBJECT[];
extern const char MSG_ID[];
extern const char PASSWORD[];
extern const char UNKNOWN_USER[];
extern const char UNKNOWN_USER_NAME[];
/* 5/10/00 key-cert support */
extern const char HEXID_MISMATCH[];
extern const char BAD_TRANS_SIG[];
extern const char KEY_CERT_CAND[];

/* ERROR and WARN messages; used by irr_check and irr_notify */
/*
 * Used as additional fields on the transaction as error
 * info lines (ie, tell the user what went wrong).
 */
extern const char ERROR_TAG[];
extern const char WARN_TAG[];

/* function prototypes */

void print_hdr_struct	(FILE *, trans_info_t *);
void free_ti_mem	(trans_info_t *);
int  parse_header	(trace_t *tr, FILE *, long *, trans_info_t *);
int  update_has_errors	(trans_info_t *);
char *myconcat		(char *, char *);
char *myconcat_nospace  (char *, char *);
int  find_token		(char **, char **);
char *free_mem		(char *);

#endif  /* _HDR_COMM_H */
