#ifndef _IRR_CHECK_H
#define _IRR_CHECK_H

#include <stdio.h>
#include <sys/types.h>
#include <regex.h>
#include "hdr_comm.h"


#define     AS_LENGTH 8 /* max length sanity check for T_AS tokens */
#define     MAX_ERROR_BUF_SIZE 2048
#define     MAX_CANON_BUF_SIZE 48000 /* this is the max size for building the canonicalized obj */
#define     MAX_TOKEN_BUF_SIZE 1024
#define     MAX_MANDS 9
/* JW take out
#define     TOTAL_SOURCES   2
*/
#define     MAX_COUNTRIES 112
/* This is good for long/rpsl
#define     ATTR_ID_LENGTH 15  eg, "route:        " */
#define     ATTR_ID_LENGTH 5  /* eg, "*rt: " */
#define     CANON_DISK 0 /* build canonicalized object to disk   */
#define     CANON_MEM  1 /* build canonicalized object in memory */
#define     CANON_LINES_MAX 1000  /* maximum number of lines an object can have */

/* Until object type is determined, set type to NO_OBJECT.
 * Set the object attribute type to F_NOATTR until a valid
 * attribute is detected.  'EMPTY_LINE_MSG' let's report_errors ()
 * know if line is an empty line msg.  When there are syntax errors,
 * we will suppress empty line messages.  When there are no syntax
 * errors, we will remove the line from the object and give
 * a warning message.
 */
#define     NO_OBJECT      -1
#define     F_NOATTR       -1
#define     EMPTY_LINE_MSG -2

/* as-in/as-out line continuation.  Insert this 'hook' into the
 * string so in the end the hook can be replaced by the continuation
 * part of the line (eg, "as-in: from AS1 1 accept").  The hook
 * will be replaced by the aforementioned text .
 */
#define LC_HOOK "@"
#define LC_CHOOK '@'
#define HOOK_LEN 1
#define lc_filter(x) (*(x) == LC_CHOOK ? (x + 1) : (x))

/* tag for line continuation */
#define 	PLUS	2
#define         SPACE	1

/* Identify message type to the error reporting routine.
 * The error routine can tally the number of messages 
 * of each type.
 */
#define     WARN_MSG           0
#define     ERROR_MSG          1
#define     INFO_MSG           2
#define     ERROR_OVERRIDE_MSG 3
#define     WARN_OVERRIDE_MSG  4
#define     EMPTY_ATTR_MSG     5

/* copy a lex token to our token buffer, strp points to the next free
 * spot in the buffer.  yyleng is incremented to account for the '\0'.
 * We want these strings '\0' terminated so that fputs () will write
 * from the buffer to disk properly (ie, fputs () requires a '\0' 
 * string terminator) (note: fputs () does not copy the '\0' to 
 * the disk file.  But this is okay because fgets () stops reading
 * at '\n' and EOF only.)
 */
#define copy_token yyleng++; \
                   memcpy (strp,&yyleng,int_size); strp += int_size; \
                   memcpy (strp,yytext,yyleng);    strp += yyleng; token_count++;\
		   curr_obj.field_count++

/* Just like copy_token above except it is called with something else
 * besides yyleng and yytext.
 */
#define copy_token_2(a,b) a++; \
                   memcpy (strp,&a,int_size); strp += int_size; \
                   memcpy (strp,b,a);  strp += a; token_count++;\
		   curr_obj.field_count++

/* Used soley for error lines and pinpointing the error causing
 * token.  So 'p' charts along the current line and points to
 * the error causing token should an error occur.
 * 
 * WARNING: incr_tokenpos () should preceed copy_token
 * since copy_token increments yyleng
 */
#define incr_tokenpos(p) p += yyleng; /*fprintf (dfile, "incr tokenpos :(%d)\n", p)*/

#define reset_token_buf(p) {int _n; canon_strp = strp = string_buf;\
                              _n = strlen (p) + 1;\
                              memcpy (strp,&_n,int_size); strp += int_size;\
			      memcpy (strp,p,_n); strp += _n;}

/* fprintf (dfile, "find_token: (strp - string_buf) (%d)\n", strp - string_buf);\ */
/* return a pointer to mth token in the token buffer */
#define find_token(p,m) {int _n, _tlen; p = string_buf; \
                         for (_n=m; _n > 1; _n--){\
                           memcpy (&_tlen,p,int_size);\
                           p += _tlen + int_size; \
                         } \
			 p += int_size; }

/* return a pointer to the 1st token in the lex token buffer */
#define first_token(p) p = string_buf; p += int_size

/* return the next token in the token buffer */
#define next_token(p) {int _tlen; p -= int_size; \
                       memcpy (&_tlen,p,int_size);\
                       p += _tlen + int_size + int_size;}

/* is the token buffer empty? */
#define token_buf_empty (strp == string_buf ? 1 : 0)

/* used to check for reoccuring fields in rs-in/rs-out fields */
enum RX_OPTS {
	IMPORT_F       = 01,
	ASPATH_TRANS_F = 02,
	FLAP_DAMP_F    = 04,
	IRRORDER_F     = 010
};
                       
/* all legal attributes */
#define     MAX_ATTRS 92
enum ATTRS {
    F_CP = 0, F_CS, F_NE, F_NP, F_RC, F_RI, F_RX, F_RR,
    F_RY, F_AC, F_AA, F_AD, F_AE, F_AH, F_AI, F_AL,
    F_AN, F_AM, F_AO, F_AS, F_AT, F_AU, F_AV, F_BG,
    F_BI, F_BL, F_CH, F_CL, F_CM, F_CO, F_CY, F_DA,
    F_DE, F_DF, F_DI, F_DM, F_DN, F_DO, F_DP, F_DT,
    F_EM, F_FX, F_GD, F_GW, F_HO, F_IF, F_II, F_IN,
    F_I6, F_IR, F_IT, F_IO, F_LA, F_LI, F_LO, F_MA,
    F_MB, F_ML, F_MT, F_MN, F_NA, F_NH, F_NI, F_NO,
    F_NS, F_NY, F_OP, F_OF, F_OM, F_OR, F_PE, F_PH,
    F_PN, F_RL, F_RM, F_RO, F_RP, F_RT, F_RZ, F_SD,
    F_SO, F_ST, F_TB, F_TC, F_TR, F_TX, F_WD, F_ZC,
    F_UE, F_UW, F_UD, F_UO
};


/* all legal object types */
#define     MAX_OBJS 13
enum OBJS {
    O_AN = 0, O_AM, O_CM, O_DN, O_IN, O_I6, O_PN, O_DP,
    O_IR, O_LI, O_MT, O_RT, O_RO
};

enum REGEX_TOKENS {
  RE_DATE = MAX_ATTRS, RE_EMAIL1, RE_EMAIL2, RE_EMAIL3,
  RE_CRYPT_PW, RE_TITLES, RE_NAME, RE_APNIC_HDL, 
  RE_LCALPHA, RE_STD_HDL, RE_RIPE_HDL, RE_COMM1, RE_COMM2, 
  RE_ASNAME, RE_ASNUM, RE_ASMACRO, RE_ARIN_HDL
};
#define REGEX_TOKEN_COUNT 17

typedef struct _canon_info_t {
  int io;             /* disk or memory canonicalization         */
  int lio;            /* disk or mem for lineptr info            */
  int do_canon;       /* do canonicalization yes or no?          */
  char *bufp;         /* next pointer                            */
  char *linep;        /* begining of line mem pointer            */
  char *buffer;       /* pointer to begining of parse buffer     */
  int buf_space_left; /* free space remaining in buffer          */
  FILE *fd;           /* pointer to canonical file on disk       */
  FILE *lfd;          /* pointer to canon line ptr info on disk  */
  FILE *efd;          /* pointer to the error line disk buffer   */
  char *flushfntmpl;  /* pointer to the flush file template name */
  char flushfn[256];  /* overflow flush buffer name              */
  char lflushfn[256]; /* lineptr overflow flush buffer name      */
  char eflushfn[256]; /* error line buffer name                  */
  long flinep;        /* begining of line disk pointer           */
  int num_objs;       /* number of objects parsed, used in QUICK_CHECK case */
} canon_info_t;

typedef struct _canon_line_t {
  short attr; /* save the attr type, need after obj is parsed to
                 determine wt of attr */
  long  count;/* increment each time attribute is seen, needed in wt
                 computation */
#define SPEC_ATTR_WEIGHT 1023
  float wt;   /* Weight of the line, lower wt's at top, heavier at bottom 
	       * The special attrs (ie, ud, ue, uw, ...) are given a special
	       * weight of 1024.  This will cause the special attr's to *always*
	       * appear at the end of the object.  Also, for syntactically correct
	       * objects the special attr's should *never* appear.  So
	       *  display_canonicalized_object () can look for the special weight
	       * of 1024 to stop displaying the object when encountered.  Finally,
	       * the parser does not know with certainty if
	       * there is a syntax error until the object has been completely parsed.
	       * So if there is a syntax error, the special attr's will be in the
	       * parse buffer and display_canonicalized_object () will show the
	       * object.
	       */
  long fpos;  /* file pos of begining of line (when line is on disk) */
  char *ptr;  /* pointer to beginning of line (when line in memory) */
  short empty_attr; /* is line and empty attr?  Want to leave if object has errors */
  long lines; /* number of '\n's in line; for use with fgets () in display_canon_obj () */
} canon_line_t;

/* The result of parsing a line can have the following 3 outcomes:
 */
enum LINE_OUTCOME {
  LEGAL_LINE   = 01,
  SYNTAX_ERROR = 02,
  EMPTY_LINE   = 04,
  DEL_LINE     = 010
};

typedef struct _parse_info_t {
  short curr_attr;	/* current attr of the line we are parsing, eg F_OR */
  int attr_lineno;	/* line number of the current attr 
			 * incremented in start_new_attr () (begin new line)
			 * incremented in do_lc_canonical ()
                              (as-in/as-out line continuation)
			 * decremented in empty_attr ()
			 * decremented in ripe181.y: spec_attr rule 
			      (want special attr's to disappear, informational only) */
  int start_lineno;     /* used to determine proper lineno for error reporting */
  int field_count;      /* number of fields for this attr */
  int num_lines;	/* number of lines this object has */
  short type;		/* object type, eg O_RT, O_MT, for route, maint */ 
  short attrs[MAX_ATTRS]; /* array of all the attr's this object has */
  int errors;		/* number of errors this object has */
  int warns;            /* number of warnings this object has */
  char lc_tag[256];     /* ai/ao line continuation tag (eg, *ao: to ... announce) */
  u_int attr_error;     /* was there an error with this attribute? */
  int error_pos;        /* position within the line of the error, the <?> symbol will go here */
  int elines;           /* number of lines in output if and error occurs */
  int eline_len;        /* number of chars in output if an error occurs */
  char *err_msg;	/* pointer to the error message buffer */
  char *errp;           /* point to position for next error msg */
  char *obj_key;        /* HDR info: object key */
  char *second_key;     /* HDR info: need for route and person obj's */
  char *op;             /* HDR info: operation, ie, DEL or OTHER */
  char *override;       /* HDR info: override password */
  char *mnt_by;         /* HDR info: all maintainer references */
  char *source;         /* HDR info: DB source */
  char *cookies;        /* HDR info: magic auth cookies */
  char *check_notify;   /* HDR info: notify attr's */
  char *check_mnt_nfy;  /* HDR info: mnt_nfy attr's */
  char *password;       /* HDR info: password for CRYPT-PW */
} parse_info_t;


/* syntax_attrs.c: syntax checking routines */

/* globals */
extern canon_line_t lineptr[];
extern int verbose;

/* JW take out
extern int eline_len;
extern int elines;
*/

extern int INFO_HEADERS_FLAG;

/* Error and warning messages go here */
char error_buf[MAX_ERROR_BUF_SIZE];

/* string lex tokens go here */
extern int int_size;
extern char string_buf[];
extern char *strp;
extern char *canon_strp;
extern FILE *dfile;
extern FILE *ofile;
extern int CANONICALIZE_OBJS_FLAG;
extern FILE *yyin;

extern char *obj_type[];
extern const char ERROR_TOKEN[];
extern short attr_is_key[MAX_ATTRS];

/* multi-valued attr checker's */
void hdl_syntax (parse_info_t *obj, char *hdl);


/* field sytnax checker's */
int email_syntax (char *email_addr, parse_info_t *obj);
void regex_syntax (char *reg_exp, parse_info_t *obj);
char *date_syntax (char *, parse_info_t *, int);
void cryptpw_syntax (char *cryptpw, parse_info_t *obj);
void source_syntax (char *source_name, parse_info_t *obj);
int is_nichdl (char *nichdl);
void nichdl_syntax (char *nic_hdl, parse_info_t *obj);
int delete_syntax (char *, parse_info_t *);
int password_syntax (char *, parse_info_t *);
int override_syntax (char *, parse_info_t *, int);
int authorise_syntax (char *, parse_info_t *, int);
int inetnum_syntax (parse_info_t *, char *, char *);
int country_syntax (char *country, parse_info_t *obj);


/* util.c */
void check_object_start (parse_info_t *obj, enum ATTRS o_type);
void check_object_end (parse_info_t *obj, canon_info_t *canon_info);
void start_new_object (parse_info_t *obj, canon_info_t *canon_info);
void error_msg_queue (parse_info_t *obj, char *emsg, int msg_type);
void report_errors (parse_info_t *obj);
void init_regexes ();
void regex_compile (regex_t re[], int offset, char *reg_exp);
int is_nichdl (char *);
int is_country (char *p, char *countries[]);
int is_special_suffix (char *);
source_t *is_sourcedb (char *, source_t *);
int starts_with_keyword (char *token);
void comm_syntax (char *comm_name, parse_info_t *obj);
char *my_strcat (int num_args, u_int skipfree, ...);
void wrap_up (canon_info_t *);
int irrorder_syntax (parse_info_t *obj, char *sources_list, char *new_source);
void convert_toupper (char *_z);
void convert_tolower (char *_z);

/* canonical.c */

void parse_buf_add (canon_info_t *canon_obj, char *format, ...);
void canonicalize_key_attr (parse_info_t *obj, canon_info_t *canon_info);
void display_canonicalized_object (parse_info_t *, canon_info_t *);
void add_canonical_error_line (parse_info_t *, canon_info_t *, int);
void start_new_canonical_line(canon_info_t *canon_info, parse_info_t *obj);
void do_lc_canonical (int *, parse_info_t *, canon_info_t *, char *);
void finish_lcline (parse_info_t *, canon_info_t *);
int irrcheck_find_token (char **x, char **y);
void set_lineptr_empty (canon_info_t *, parse_info_t *);

/* header_construction.c */

void build_header_info (parse_info_t *);
void display_header_info (parse_info_t *);
char *my_concat (char *, char *, int);

/* ripe181.fl */
void reset_token_buffer ();

/* prefix.c */
int _is_ipv4_prefix (parse_info_t *obj, char *string, int short_prefix);
int net_mask_syntax (parse_info_t *obj, char *prefix);
int irrd_inet_pton (int af, const char *src, void *dst);

#endif /* IRR_CHECK_H */
