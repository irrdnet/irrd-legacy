/*
 * $Id: irr_notify.h,v 1.8 2002/10/17 19:41:44 ljb Exp $
 */

#ifndef _IRR_NOTIFY_H
#define _IRR_NOTIFY_H

#include <stdio.h>
#include <sys/types.h>
#include <hdr_comm.h>
#include <config.h>
#include <pipeline_defs.h>

/* RFC 2769 RPS-DIST: offset from UTC, used in creating timestamps */
#define UTC_OFFSET "+04:00"
/* char storage size for notify and forward addr's */
#define MAX_ADDR_SIZE 5000

/* max number of open file handles */
#define MAX_HDLS 200
/* related to MAX_HDLS, MAX_NDX_SIZE gives the max
 * number of file hdl's a list may have.  Should
 * always be same as MAX_HDLS
 */
#define MAX_NDX_SIZE 200
			   
/* Idenify the 3 different notify response types */
enum NOTIFY_T {
  NOTIFY_RESPONSE, FORWARD_RESPONSE, SENDER_RESPONSE
};

/* transaction outcome from IRRd */
#define _SUCCESS   '1'
#define _NET_ERROR '2'
#define _NOOP      '3'
#define _SKIP      '4'
/* SKIP_RESULT object ok, skip because of error semantics */
/* USER_ERROR means a syntax, auth, ... error occured.
 * SKIP_ERROR menas the trans was skipped but there were
 * no user errors.  
 */
enum SERVER_RES {
  SUCCESS_RESULT        = 01, 
  SKIP_RESULT           = 02, 
  IRRD_ERROR_RESULT     = 04, 
  INTERNAL_ERROR_RESULT = 010,
  NOOP_RESULT           = 020,
  USER_ERROR            = 040, 
  NULL_SUBMISSION       = 0100
};

typedef struct _irrd_result_t {
  enum SERVER_RES svr_res;
  char *err_msg;
  char *op;
  char *obj_type;
  char *obj_key;
  char *keycertfn;
  char *source;
  long offset;
  u_int hdr_fields;
  struct _irrd_result_t *next;
} irrd_result_t;

typedef struct _ret_info_t {
  irrd_result_t *first;
  irrd_result_t *last;
} ret_info_t;

typedef struct _nfile_t {
  FILE *fp;
  char *fname;
} nfile_t;


/* temp file name for notification files */
extern char tempfname[];

/* Sender response text */

extern const char SENDER_HEADER[];
extern const char FORWARD_HEADER[];
extern const char NOTIFY_HEADER[];
extern const char DIAG_HEADER[];
extern const char MAIL_HEADERS[];
extern const char WEB_UPDATE[];
extern const char REPLACE_FAIL_NOTE[];
extern const char RESPONSE_FOOTER[];
extern const char SENDER_OP_FAILED[];
extern const char SENDER_OP_SUCCESS[];
extern const char SENDER_OP_NOOP[];
extern const char NULL_SUBMISSION_MSG[];
extern const char SENDER_NET_ERROR[];
extern const char SENDER_SKIP_RESULT[];
extern const char INTERNAL_ERROR[];
extern const char ERROR_TAG[];
extern const char DEL_NO_EXIST_MSG[];
extern const char MAINT_NO_EXIST_MSG[];
extern const char AUTHFAIL_MSG[];
extern const char NEW_MNT_ERROR_MSG[];
extern const char NEW_MNT_ERROR_MSG_2[];
extern const char DEL_MNT_ERROR_MSG[];
extern const char DEL_MNT_ERROR_MSG_2[];
extern const char BAD_OVERRIDE_MSG[];
extern const char UNKNOWN_USER_MSG[];
extern const char PREV_OBJ_MSG[];
extern const char EXIST_OBJ_MSG[];
extern const char NOTIFY_REPL_MSG[];
extern const char NOTIFY_DEL_MSG[];
extern const char NOTIFY_ADD_MSG[];
extern const char MSG_SEPERATOR[];
extern const char FORWARD_REPL_MSG[];
extern const char FORWARD_DEL_MSG[];
extern const char FORWARD_ADD_MSG[];

/* globals */
extern char notify_addrs[];
extern char *nnext;
extern int nndx[];
extern int num_notify;

extern char forward_addrs[];
extern char *fnext;
extern int fndx[];
extern int num_forward;

extern char sender_addrs[];
extern char *snext;
extern int sndx[];
extern int num_sender;

extern nfile_t msg_hdl[];
extern int num_hdls; 

/* function prototypes */



void notify (trace_t *, char *, FILE *, int, 
	     int, int, char *, int, char *, int, char *, char *, char *, 
	     char *, long, FILE *, char *);
void send_notifies (trace_t *, int, FILE *, int);
int present_in_list (char *, char *, char *);
void build_notify_responses (trace_t *, char *, FILE *, trans_info_t *, 
			     irrd_result_t *, char *, int, int);


/* util.c */
char *dup_single_token (char *);
int is_valid_op (char *);
int chk_hdr_flds (u_int);

int get_list_members (char *, char *, char **);
void remove_sender (char *, char *, char **);

u_int int_type_field (char *, trans_info_t *);
u_int list_members_field (char *, trans_info_t *);
u_int dup_token_field (char *, trans_info_t *);
u_int is_from_field (char *, trans_info_t *);
u_int is_maint_no_exist_field (char *, trans_info_t *);
int   put_transaction_code (trace_t *, irrd_result_t *, char *);
int   put_transaction_code_new (trace_t *, irrd_result_t *, char *);
int   is_keycert_obj (irrd_result_t *);
void  update_pgp_ring (trace_t *, irrd_result_t *, char *);
void  update_pgp_ring_new (trace_t *, irrd_result_t *, char *);
void  skip_transaction (FILE *, long *);
void  chk_email_fields (trans_info_t *);
int   update_trans_outcome_list (trace_t *, ret_info_t *, trans_info_t *, long , 
				enum SERVER_RES, char *);
void  reinit_return_list (trace_t *, ret_info_t *, enum SERVER_RES);
int   pick_off_header_info (trace_t *, FILE *, int, ret_info_t *, int *);

#include <irrd_ops.h>
#endif /* _IRR_NOTIFY_H */
