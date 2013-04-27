#ifndef _IRR_RPSL_CHECK_H
#define _IRR_RPSL_CHECK_H

#include <regex.h>
#include <hdr_comm.h>
#include <irrauth.h>
#include <pgp.h>

#define     AS_LENGTH 8               /* max length sanity check for 
				       * T_AS tokens 
				       */
#define     MAX_ERROR_BUF_SIZE 2048
#define     MAX_CANON_BUF_SIZE 480000 /* this is the max size for building 
				       * the canonicalized obj 
				       */
#define     MAX_ATTR_SIZE 66560       /* maximum chars for an attribute */

#define     MAX_COUNTRIES 241	/* number of countries in countries array */
#define     ATTR_ID_LENGTH 12	/* number of positions indent attribute vals */
#define     CANON_DISK 0 /* build canonicalized object to disk   */
#define     CANON_MEM  1 /* build canonicalized object in memory */
#define     CANON_LINES_MAX 1000  /* maximum number of lines an object can have */
#define     MAX_DATA_TYPES 4 /* dictionary: # element's in data_typep[] */

/* Until object type is determined, set type to NO_OBJECT.
 * Set the object attribute type to F_NOATTR until a valid
 * attribute is detected.  'EMPTY_LINE_MSG' let's report_errors ()
 * know if line is an empty line msg.  When there are syntax errors,
 * we will suppress empty line messages.  When there are no syntax
 * errors, we will remove the line from the object and give
 * a warning message.
 */
#define     NO_OBJECT      -1
#define     EMPTY_LINE_MSG -2

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

#include "irr_rpsl_tokens.h"

/* data from the ripedb.config file */

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

#define MAX_RESERVED_PREFIXES 5
#define MAX_RESERVED_WORDS 36

enum REGEX_TOKENS {
  RE_DATE = MAX_ATTRS, RE_EMAIL1, RE_EMAIL2, RE_EMAIL3,
  RE_CRYPT_PW, RE_TITLES, RE_NAME, RE_APNIC_HDL, 
  RE_LCALPHA, RE_STD_HDL, RE_RIPE_HDL, RE_COMM1, RE_COMM2, 
  RE_ASNAME, RE_ASNUM, RE_ARIN_HDL, RE_REAL,
  RE_SANITY_HDL
};
#define REGEX_TOKEN_COUNT 19

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
  short skip_attr; /* empty or machine gen attr, leave if object has errors */
  long lines; /* number of '\n's in line; for use with fgets () in display_canon_obj () */
} canon_line_t;

/* The result of parsing a line can have the following 4 outcomes:
 */
enum LINE_OUTCOME {
  LEGAL_LINE       = 01,
  SYNTAX_ERROR     = 02,
  EMPTY_LINE       = 04,
  DEL_LINE         = 010,
  MACHINE_GEN_LINE = 020
};

/* Union types: specialized info depending on object type */

enum PI_INFO_TYPE {
  EMPTY = 0, KEY_CERT
};

typedef struct _key_cert_t {
  char kchexid[9];
  char *certif;
  char *owner;
  char *fingerpr;
  int owner_count;
} key_cert_t;

typedef struct _parse_info_t {
  short type;		/* object type, eg O_RT, O_MT, for route, maint */ 
  short curr_attr;	/* current attr, eg F_RT, R_MT */
  int start_lineno;     /* line number of attr start on outputed object */
  int num_lines;	/* index into the lineptr[] array.  because of line 
			 * continuation a lineptr[] line can have more than 
			 * one '\n'.  each lineptr[] line is ended with a '\0'. */
  int attr_too_big;     /* attr exceeds maximum length */
  int field_count;      /* number of fields for this attr */
  short attrs[MAX_ATTRS]; /* array of all the attr's this object has */
  int errors;		/* number of errors this object has */
  int warns;            /* number of warnings this object has */
  char lc_tag[256];     /* ai/ao line continuation tag (eg, *ao: to ... announce) */
  u_int attr_error;     /* was there an error with this attribute? */
  int error_pos;        /* position within the line of the error, the <?> symbol will go here */
  int elines;           /* number of lines in output if an error occurs */
  int eline_len;        /* number of chars in output if an error occurs */
  char *err_msg;	/* pointer to the error message buffer */
  char *errp;           /* point to position for next error msg */
  enum PI_INFO_TYPE union_type; /* data tag for union types */
  union {               /* specialized info varying by object type */
    key_cert_t kc;      /* key_cert object info */
  } u;
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
  char *keycertfn;      /* HDR info: key certificate file name */
} parse_info_t;


/* ---------------RPSL Dictionary data struct's----------*/

/* this should be exactly one char in length, see append_enum () */
#define ENUM_DELIMIT ':'

enum PROTO_TYPE {
  MANDATORY, OPTIONAL, NEITHER
};

enum RPSL_DATA_TYPE {
  UNKNOWN = -1, PREDEFINED, UNION, TYPEDEF, LIST
};

#define MAX_PREDEF_TYPES 20
enum PREDEF_TYPE {
  INTEGER = 0, REAL, ENUM, STRING, BOOLEAN, RPSL_WORD, FREE_TEXT, EMAIL,
  AS_NUMBER, IPV4_ADDRESS, IPV6_ADDRESS, ADDRESS_PREFIX, ADDRESS_PREFIX_RANGE,
  DNS_NAME, FILTER, AS_SET_NAME, ROUTE_SET_NAME, RTR_SET_NAME,
  FILTER_SET_NAME, PEERING_SET_NAME
};

enum METHOD_TYPE {
  RPSL_OPERATOR, USER_METHOD, UNION_OF_METHODS
};

enum ARG_CONTEXT {
  LIST_TYPE, NON_LIST_TYPE
};

/* New................................. */
typedef struct _typedef_t {
  char *name;
  struct _type_t *t;
} typedef_t;

typedef struct _irange_t {
  unsigned long upper;
  unsigned long lower;
} irange_t;

typedef struct _drange_t {
  double upper;
  double lower;
} drange_t;

typedef struct _predef_t {
  enum PREDEF_TYPE ptype;
  int use_bounds;
  union {
    irange_t i;
    drange_t d;
  } u;
  char *enum_string;
} predef_t;

typedef struct _union_t {
  struct _param_t *ll;
} union_t;

typedef struct _param_t {
  struct _type_t *t;
  struct _param_t *next;
} param_t;

typedef struct _type_t {
  enum RPSL_DATA_TYPE type;
  int  list;
  long  min_elem;
  long  max_elem; /* if max_elem > 0 then (min_elem <= # args <= max_elem) */
  union {
    typedef_t t;
    predef_t  p;
    union_t   u;
  } u;
  struct _type_t *next;
} type_t;

typedef struct _method_t {
  enum METHOD_TYPE type;
  enum ARG_CONTEXT context;
  char  *name;
  struct _param_t *ll;
  struct _method_t *umeth;
  struct _method_t *next;
} method_t;

typedef struct _rp_attr_t {
  char       *name;
  struct _method_t   *first;
  struct _rp_attr_t  *next;
} rp_attr_t;

typedef struct _type_t_ll {
  struct _type_t *first;
  struct _type_t *last;
} type_t_ll;

typedef struct _rp_attr_t_ll {
  struct _rp_attr_t *first;
  struct _rp_attr_t *last;
} rp_attr_t_ll;

typedef struct _proto_t {
  char *name;
  enum PROTO_TYPE type;
  struct _method_t *first;
  struct _proto_t  *next;
} proto_t;

typedef struct _proto_t_ll {
  struct _proto_t *first;
  struct _proto_t *last;
} proto_t_ll;

typedef struct _afi_t {
  char *name;
  struct _afi_t  *next;
} afi_t;

typedef struct _afi_t_ll {
  struct _afi_t *first;
  struct _afi_t *last;
} afi_t_ll;

typedef struct _ph_t { /* place holder */
  struct _method_t *method;
  struct _param_t  *parm;
  struct _type_t   *type;
  char *en;
  char *cs;
} ph_t;

/* create memory for the place holder struct */
#define create_placeholder(p) p  = (ph_t *) malloc (sizeof (ph_t))

/* New..............................*/

/* syntax_attrs.c: syntax checking routines */

int xx_set_syntax (char *, char *);
int todays_date   ();

/* globals */
extern canon_line_t lineptr[];
extern int          verbose;
extern int          attr_tokens[];
extern int          reserved_word_token[];
extern char         *reserved_word[];
extern char         *reserved_prefix[];
extern char         *rp_word[];
extern type_t_ll    type_ll;
extern rp_attr_t_ll rp_attr_ll;
extern proto_t_ll   proto_ll;
extern afi_t_ll     afi_ll;
extern char         *predef_type[];
extern char         RPSL_dictionary[];
extern int          parse_RPSL_dictionary;
extern char         *data_type[];
extern int          start_new_line;
extern short        legal_attrs[MAX_OBJS][MAX_ATTRS];
extern char         *attr_name[MAX_ATTRS]; 
extern const char   tmpfntmpl[];

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

int  email_syntax	(char *, parse_info_t *);
void regex_syntax	(char *, parse_info_t *);
char *date_syntax	(char *, parse_info_t *, int);
char *time_interval_syntax (parse_info_t *, char *, char *, char *, char *);
void cryptpw_syntax	(char *, parse_info_t *);
void source_syntax	(char *, parse_info_t *);
int  is_nichdl		(char *);
void nichdl_syntax	(char *, parse_info_t *);
int  delete_syntax	(char *, parse_info_t *);
int  password_syntax	(char *, parse_info_t *);
int  inetnum_syntax	(parse_info_t *, char *, char *);
int  country_syntax	(char *, parse_info_t *);
int  asnum_syntax	(char *, parse_info_t *);
void mb_check		(parse_info_t *, char *);
char *hexid_check	(parse_info_t *);
int  get_fingerprint	(parse_info_t *,  char *, pgp_data_t *);

/* util.c */

void check_object_end	(parse_info_t *, canon_info_t *);
void start_new_object	(parse_info_t *, canon_info_t *);
void error_msg_queue	(parse_info_t *, char *, int);
void report_errors	(parse_info_t *);
void init_regexes	();
void regex_compile	(regex_t [], int, char *);
int  is_nichdl		(char *);
int  is_country		(char *, char *[]);
int  is_special_suffix	(char *);
source_t *is_sourcedb	(char *, source_t *);
char *my_strcat		(parse_info_t *, int, u_int, ...);
void wrap_up		(canon_info_t *);
int  irrorder_syntax	(parse_info_t *, char *, char *);
void convert_toupper	(char *);
void convert_tolower	(char *);
int  is_reserved_word	(char *);
int  has_reserved_prefix (char *);
rp_attr_t *is_RPattr	(rp_attr_t_ll *, char *);
method_t  *find_method	(method_t *, char *);
method_t  *find_proto_method  (proto_t *, char *);
void rpsl_lncont	(parse_info_t *, canon_info_t *, int);
void save_cookie_info	(parse_info_t *, char *);
void rm_tmpdir		(char *);
void add_machine_gen_attrs (parse_info_t *, canon_info_t *);
char *todays_strdate     ();

/* canonical.c */

void parse_buf_add	(canon_info_t *, char *, ...);
void canonicalize_key_attr	(parse_info_t *, canon_info_t *, int);
void display_canonicalized_object (parse_info_t *, canon_info_t *);
void add_canonical_error_line	(parse_info_t *, canon_info_t *, int);
void start_new_canonical_line	(canon_info_t *, parse_info_t *);
int  irrcheck_find_token	(char **, char **);
void set_skip_attr	(canon_info_t *, parse_info_t *);

/* hdr_build.c */

void build_header_info	(parse_info_t *, char *);
void display_header_info (parse_info_t *);
char *my_concat		(char *, char *, int);

/* data dictionary */

int  valid_args		(parse_info_t *, method_t *, int, char *, int, char **);
char *append_enum	(char *, char *);
param_t *create_parm	(type_t *);
param_t *add_parm_obj	(param_t *, param_t *);
type_t *create_type	(parse_info_t *, enum RPSL_DATA_TYPE, ...);
type_t *create_union	(param_t *);
type_t *create_predef	(enum PREDEF_TYPE, int, irange_t *i, drange_t *, char *);
type_t *create_typedef	(char *, type_t *);
method_t *create_method	(char *, param_t *, enum ARG_CONTEXT, int);
method_t *add_method	(method_t *, method_t *);
rp_attr_t *create_rp_attr	(char *, method_t *);
void add_rp_attr	(rp_attr_t_ll *, rp_attr_t *);
proto_t *create_proto_attr	(char *, method_t *);
void add_new_proto		(proto_t_ll *, proto_t *);
proto_t *find_protocol		(proto_t_ll *, char *);
afi_t *create_afi_attr	(char *);
void add_new_afi	(afi_t_ll *, afi_t *);
afi_t *find_afi	(afi_t_ll *, char *);
void print_typedef_list (type_t_ll *);
void print_parm_list    (param_t *);
void print_predef       (type_t *);
void print_union	(type_t *);
void print_typedef	(type_t *);
void print_method_list	(method_t *);
void print_rp_list	(rp_attr_t_ll *);
void print_proto_list	(proto_t_ll *);
void print_afi_list	(afi_t_ll *);
enum ARG_CONTEXT find_arg_context (param_t *, int);

/* rpsl.fl */
void reset_token_buffer ();

/* prefix.c */
int _is_ipv4_prefix (parse_info_t *, char *, int);
int _is_ipv6_prefix (parse_info_t *, char *, int);
int irrd_inet_pton  (int, const char *, void *);

#endif /* IRR_RPSL_CHECK_H */
