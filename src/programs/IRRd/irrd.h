/*
 * $Id: irrd.h,v 1.27 2001/08/06 17:33:59 ljb Exp $
 * originally Id: irrd.h,v 1.94 1998/08/03 17:29:07 gerald Exp 
 */


#ifndef _IRRD_H
#define _IRRD_H

#ifdef USE_GDBM
#include <gdbm.h> 
#endif  /* USE_GDBM */
#ifdef USE_DB1
#include <db.h>
#endif
#include <radix.h>
#include <dirent.h>
#ifndef NT
#include <regex.h>
#endif /* NT */
#include "scan.h"
#include "hash.h"
#include "timer.h"
#include "rwlock.h"
#include <config.h>
#ifndef NT
#include <irr_defs.h>
#endif /* NT */

#define IRRD_FAST_BUF_SIZE   1024*400

#define MIRROR_TIMEOUT 600 /* 10 minutes */

#define DEF_FTP_URL "ftp://ftp.radb.net/radb/dbase"

enum SPEC_KEYS { /* these are used for id values in the special hash */
  AS_MACROX,	/*  hash lookup for as-macro objects (RIPE181 only) */
  COMM_LISTX,   /*  !h community (route) list lookups (RIPE181 only) */
  SET_OBJX,	/*  hash lookup for as-set and rs-set objects (RPSL only) */
  SET_MBRSX,	/*  hash lookup for autnum's/route's which reference 
		     an as-set/rs-set (RPSL only) */
  GASX,		/*  hash lookup for !gas queries */
  ROUTE_BITSTRING,   /* used for storing radix on disk as 11111011 */
  DATABASE_STATS    /* store useful information like number of objects in spec_dbm */
};

/* used by fetch_hash_spec(), tell's what to do with fetched value:
to unfurl or not to unfurl, that is the question */
enum FETCH_T {
  FAST,   /* fetch unpacked value portion, ie, for !gas queries */
  UNPACK /* unpack the value portion of hash, used by indexes */
}; 

enum INDEX_T {
  DISK_INDEX, 	/* index points to disk */
  MEM_INDEX	/* index point to memory */
};

enum ANSWER_T {
  PRECOMP_OBJ,  /* answer is a char *, don't add '\n' and no field conversion */
  DB_OBJ      
};

enum RIPE_FLAGS_T {
  FAST_OUT    = 01, 	/* fast output, short fields, no recurse lookup */
  RECURS_OFF  = 02, 	/* no recurse lookup */
  LESS_ONE    = 04,	/* one level less route search */
  LESS_ALL    = 010,	/* all levels less route search */
  MORE_ONE    = 020,    /* one level more route search */
  MORE_ALL    = 040,    /* all levels more route search */
  SOURCES_ALL = 0100,   /* set sources to all */
  SET_SOURCE  = 0200,   /* set source <DB> */
  TEMPLATE    = 0400,   /* send object template <type> */
  OBJ_TYPE    = 01000   /* restrict object search to <type> */
};
#define MAX_RIPE_FLAGS 10
#define IRR_MAX_MCMDS  32

/* RPSL only 
#define	NO_SYNTAX		-1
#define RIPE181			0
#define RPSL			1
*/
enum DB_SYNTAX { /* Do not change the order!!!
		  * RIPE181 must be 0 and RPSL 1
		  */
  RIPE181 = 0, RPSL, MIXED , UNRECOGNIZED, EMPTY_DB 
};

enum REMOTE_MIRROR_STATUS_T {
  MIRRORSTATUS_UNDETERMINED = 0,  /* Uninitialized */
  MIRRORSTATUS_FAILED,            /* Failed to connect */
  MIRRORSTATUS_UNSUPPORTED,       /* Not supported */
  MIRRORSTATUS_UNAVAILABLE,       /* DB doesn't exist, or administratively unavailable */
  MIRRORSTATUS_YES,               /* Mirrorable */
  MIRRORSTATUS_NO                 /* Not mirrorable, but we're going to say something */
};


typedef struct _irr_route_object_t {
  struct _irr_attr_t	*next, *prev;	/* linked_list -- multiple routes for prefix */
  u_short	origin;
  u_char	withdrawn;
  u_long	offset;
  u_long        len;
} irr_route_object_t;


typedef struct _irr_database_t {
  struct _irr_database_t	*next, *prev;	/* for linked_list */
  char			*name;		/* radb, mci, whatever */  
  FILE			*fd;		/* database.db file fd */
  int			journal_fd;     /* database.JOURNAL file fd */
  int			bytes;		/* bytes read so far */
  u_long		max_journal_bytes;  /* number of bytes in journal log */
  enum DB_SYNTAX        db_syntax;      /* empty, rpsl or ripe181 */
  u_long                obj_filter;     /* object bit-fields of 1 are filtered out */

  char			*read_buf;
  char			*read_cur_ptr;
  char			*read_end_ptr;
  char			last_char;
  

  /* mirroring stuff */
  int			mirror_fd;	/* the temporary fd for remote mirroring */
  FILE			*mirror_disk_fd; 
  int			mirror_update_size;
  long			time_last_successful_mirror;
  u_long		serial_number;	/* serial number for mirroring */
  u_long		new_serial_number;	/* serial number for mirroring */
  prefix_t		*mirror_prefix; /* prefix of host to connect for mirroring */
  char			*remote_ftp_url; /* for irrdcacher to fetch database */
  int                   rpsdist_flag;    /* if set, this DB was created with
					  * a 'rpsdist_database' command */
  int                   rpsdist_auth;    /* rpsdist authoritative */
  int                   rpsdist_trusted; /* rpsdist trusted */
  int                   rpsdist_port;    /* rpsidst port */
  char                  *rpsdist_host;   /* rpsdist host */
  char                  *rpsdist_accept_host;/* accept connections for this host */
  char                  *repos_hexid;    /* used by rps-dist to verify floods */
  char                  *pgppass;        /* password used for signing floods */
  int			mirror_port;
  char			mirror_error_message[1024];

  u_long		remote_oldestjournal; /* What is their !j? */
  u_long		remote_currentserial; /* What is their !j? */
  u_long                remote_lastexport;    /* !j last export */
  enum REMOTE_MIRROR_STATUS_T  remote_mirrorstatus;

  u_long		flags;		/* IRR_READ_ONLY, etc */
#define IRR_AUTHORITATIVE	1	/* we are the master copy -- this can be updated */
#define IRR_READ_ONLY		2
#define IRR_NODEFAULT		4       /* Do not include by default in queries */

  u_long		access_list;		/* restrict access */
  u_long		write_access_list;	/* restrict writes -- refines access */
  u_long		mirror_access_list;     /* restrict mirror -- refines access */

  pthread_mutex_t	mutex_lock;
  /*rwlock_t		rwlock;*/
  radix_tree_t		*radix;
  HASH_TABLE		*hash;		
#ifdef USE_GDBM
  GDBM_FILE		dbm;		/* disk hash (dbm files) */
  GDBM_FILE             dbm_spec;       /* disk hash for special indexing */
#endif /* USE_GDBM */
#ifdef USE_DB1
  DB                    *dbm;
  DB                    *dbm_spec;
#endif
  HASH_TABLE		*hash_spec;       /* hash for special queries */
  HASH_TABLE		*hash_spec_tmp;	  /* memory hash */

  int			no_dbclean;	/* flag to disable dbcleaning. By default, we clean */
  mtimer_t		*mirror_timer;
  mtimer_t		*clean_timer;
  mtimer_t		*export_timer;

  char			*export_filename; /* database name if different */

  
  /* statistics */
  int			num_objects[IRR_MAX_KEYS +2];

  /* mirror status and statistics */
  time_t		time_loaded;		/* when the db was loaded (or reloaded) */
  time_t		last_mirrored;		/* when we last mirrored successfully! */
  time_t		last_update;		/* last email/TCP update */
  time_t		mirror_started;		/* hook for us to timeout on */
  int			num_changes;
  int			num_objects_deleted[IRR_MAX_KEYS +2];
  int			num_objects_changed[IRR_MAX_KEYS +2];
  int			num_objects_notfound[IRR_MAX_KEYS +2];
} irr_database_t;

typedef struct _irr_answer_t {
  FILE          *fd;
  enum IRR_OBJECTS type;
  enum DB_SYNTAX db_syntax;
  u_long        offset;
  u_long        len;
  char 		*blob;
} irr_answer_t;


/* a generic object so we don't have to write new code every time a new
 * object type is added 
 */
typedef struct _irr_object_t {
  FILE		*fd;		/* the file we were read from */
  enum IRR_OBJECTS type;
  char		*name;		/* primary key */
  int		mode;		/* ADD, DELETE, UPDATE */
  u_long	offset;
  u_long	len;
  LINKED_LIST	*ll_keys;	/* list of secondary keys and types */
  u_int         filter_val;     /* used in filtering out certain object types */

  /* convenience stuff */
  int		withdrawn;
  u_short	 origin;	/* use in route object */
  char          *nic_hdl;       /* secondary key */
  LINKED_LIST	*ll_community;	/* use in route object */
  LINKED_LIST	*ll_as;		/* for us in asmacro */
  LINKED_LIST	*ll_prefix;	/* prefix (as ascii string) for IPv6 site objects */
  LINKED_LIST   *ll_mbr_of;     /* RPSL route and aut-num for set inclusion */
  LINKED_LIST   *ll_mbr_by_ref; /* RPSL route-set and as-set */
  LINKED_LIST   *ll_mnt_by;     /* RPSL routes and as's */
} irr_object_t;

typedef struct _hash_spec_t {
  char *key;
  enum SPEC_KEYS id;
  LINKED_LIST	 *ll_1;
  LINKED_LIST	 *ll_2;
  u_long         len1, len2;    /* keep track of gas char length */
  u_long	 items1, items2;/* number of gas prefixes in answer */
  char *unpacked_value;         /* used by !gas for fast output, points
                                   to the hash_item->value item */
  char *gas_answer;             /* just a pointer into unpacked_value */
} hash_spec_t;

/* struct for collecting a key and key type
 * for irr_database_find_matches() lookup */
typedef struct _reference_key_t {
  char *key;
  enum IRR_OBJECTS type;
} reference_key_t;

typedef struct _statusfile_t {
  char                  *filename;        /* Name of the status file */
  HASH_TABLE		*hash_sections;   /* Hash of variable hashes keyed on section */
  trace_t               *trace_status;    /* Trace handle for logging exceptions */
  pthread_mutex_t	mutex_lock;       /* Mutex on this file */
} statusfile_t;

/* the main, global data structure
 * holds user specified values and defaults
 */
typedef struct _irr_t {
  char			*database_dir;
  char			*ftp_dir;	  /* location where we put database for ftp */
  char			*tmp_dir; 	  /* cache directory for writing temp files quickly */
  char                  *path; /* additional path componenet, eg, find irrdcacher */
  /* LINKED_LIST		*ll_database_tmp; use for sorting */
  LINKED_LIST		*ll_database;	  /* list of databases */
  LINKED_LIST		*ll_database_alphabetized; /* just used in show database */
  LINKED_LIST		*ll_connections;  /* all current whois connections */
  pthread_mutex_t	connections_mutex_lock; /* lock around connection linked list */
  int			sockfd;		  /* the whois/port 43 socket */
/*  int			access_list;	   access list before accepting telnets */
  int			irr_port;	  /* The port for RAWhoisd connections */
  int			irr_port_access;  /* access list before accepting telnets */
  int			whois_port;	  /* whois UDP queries */
  int			whois_port_access;
  int			mirror_interval;  /* Default seconds between getting mirrors */
  int			max_connections;  /* the max num of simultaneous RAWhoisd conn */
  int			connections;	  /* current number of connections */
  int			use_disk;	  /* should we use dbm/gdbm files? */
  int			use_cache;	  /* cache gas answers */
  enum DB_SYNTAX	database_syntax;  /* ripe181 or rpsl */
  u_long		export_interval;  /* when should we export database */
  int			rebuild_indicies; /* flag if we should rebuild dbm files */
  pthread_mutex_t	lock_all_mutex_lock;
  HASH_TABLE		*key_string_hash; /* fast key lookup (*rt, *am, etc) */

  statusfile_t          *statusfile;      /* Global status file */
 
 /* stuff just to keep track of pipeline */
  trace_t		*submit_trace;
  char			*db_admin;
  char			*pgp_dir;
  char			*override_password;
  char			*irr_host;
  LINKED_LIST		*ll_response_footer;
  /* end stuff just to keep track of pipeline */
} irr_t;

typedef struct _final_answer_t {
  u_char *ptr;
  u_char *buf;
} final_answer_t;


typedef struct _irr_connection_t {
  struct _irr_connection_t *next, *prev;
  int			sockfd;		/* the TCP socket we write/read from */
  int			scheduled_for_deletion;
  schedule_t		*schedule;
  prefix_t		*from;
  u_long		start;		/* time (UTC) when connection started */
  LINKED_LIST		*ll_database;
  LINKED_LIST           *ll_answer;
  LINKED_LIST		*ll_final_answer;
  char buffer[BUFSIZE];
  char			*answer;
  int			answer_len;
  /* stuff for recieving updates -- need to preserve state */
  int			state;
  u_short		withdrawn;      /* include withdrawn routes? */
  u_short		short_fields;   /* short fields? (eg, *rt vs route ) */
  u_short		stay_open;	/* default to one-shot, !! to stay open */
  u_short               full_obj;       /* show/display full object? default yes */
  u_int			ripe_flags;     /* list of flags for ripe commands */
  enum IRR_OBJECTS      ripe_type;      /* used for -t and -T ripe flags */
  char                  ripe_tmp[BUFSIZE];
  FILE			*update_fd;
  char			update_file_name[BUFSIZE];
  irr_database_t	*database;
  u_long		timeout;	/* seconds before idle connection times out */	

  char tmp[BUFSIZE];            
  char *cp;			        /* pointer to cursor in line */
  char *end;			       /* pointer to end of line */
  char                  *ENCODING;    /* the type of ENCODING: plain, gzip */
} irr_connection_t;


/* for scan.c quick matching of *rt, *am, etc */
typedef struct _keystring_hash_t {
  char *key;
  int  num;
} keystring_hash_t;


typedef struct _hash_item_t {
  char *key;
  char *value;
} hash_item_t;

typedef struct _section_hash_t {
  char *key;
  HASH_TABLE *hash_vars;
} section_hash_t;

typedef struct _gas_hash_item_t {
  char as[10];
  LINKED_LIST *ll_prefix;
} gas_hash_item_t;

typedef struct _key_label {
  char *name;
  int len;
  enum F_PROPERTY f_type;
  enum OBJECT_FILTERS filter_val; /* identify obj field type for filtering */
} key_label_t;

typedef struct _m_command_t {
  char *command;
  enum IRR_OBJECTS type;
} m_command_t;

typedef struct _find_filter_t {
  char *name;
  enum OBJECT_FILTERS filter_f;
} find_filter_t;

typedef struct _ll_as_sets_t {
  char *name;
  LINKED_LIST *ll_aut_nums;
} ll_as_sets_t;


typedef struct _radix_str_t {
  struct _radix_str_t *next, *prev;
  radix_node_t *ptr;
} radix_str_t;

typedef struct _irr_hash_string_t {
  struct _irr_hash_string_t *next, *prev; /* linked_list -- multiple routes for prefix */
  char *string;
} irr_hash_string_t;

extern irr_t IRR;
extern trace_t *default_trace;

#define	IRR_OUTPUT_BUFFER_SIZE  1024*64
#define IRR_DEFAULT_PORT	43
#define IRR_TMP_DIR             "/var/tmp"
#define IRR_EXIT		2


#define	MIRROR_BUFFER		1024*4

#define IRR_DELETE		2
#define IRR_UPDATE		3		/* implicit replace -- need to delete first */
#define IRR_NOMODE		4               /* serial xtrans with no ADD or DEL header */
#define IRR_ERRMODE		5		/* illegible serial */

#define IRR_MODE_LOAD_UPDATE    1

/* search types */
#define SEARCH_ONE_LEVEL	0		/* e.g. !rxx.xx.xx,l */
#define SEARCH_ALL_LEVELS	1

#define NO_SHOW_ANSWER		0		/* we have this flag cause whois */
#define SHOW_ANSWER		1


#define SHOW_FULL_OBJECT	0
#define SHOW_JUST_ORIGIN	1		/* !r141.211.128/24,o */

#define IRR_MAX_JOURNAL_SIZE	4096*1000	/* maxmimum size of journal files */
/* journal file names are "IRR.database_dir/db_name.JOURNAL[.1]
 * eg, /user/joe/irrd/cache/radb.JOURNAL
 */
#define	SJOURNAL_NEW	        "JOURNAL"       /* new journaling file ext */
#define SJOURNAL_OLD            "JOURNAL.1"     /* old journaling file ext */
#define	JOURNAL_NEW	        0
#define JOURNAL_OLD             1

/* 
 * secondary or primary indicies 
 */
#define PRIMARY			1
#define SECONDARY		2
#define BOTH			3

/* match flags */
#define RIPEWHOIS_MODE          1
#define RAWHOISD_MODE           2              /* means:
                                                  1. !m... commands lock db's
						  2. !m... commands return after
						     first match
						*/
#define PRIMARY_MODE		4	/* search hash for primary only */
#define TYPE_MODE		8	/* search hash for exact type */

/***********************/
					   
/* atomic transaction support */

extern int atomic_trans;

typedef struct _trans_t {
  irr_database_t *db;
  char *tfile;
  struct _trans_t *next;
} trans_t;

typedef struct _rollback_t {
  trans_t *first;
  trans_t *last;
} rollback_t;

#include "irrd_prototypes.h"

#ifndef MAX
#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#endif

#endif /* IRRD_H */
