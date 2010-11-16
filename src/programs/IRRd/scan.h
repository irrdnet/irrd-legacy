
#ifndef _SCAN_H
#define _SCAN_H

#define whitespace_remove(p)  {char *_z; \
 while ((*(p) == ' ') || (*(p) == '\t')) (p)++; \
 _z = (p) + strlen ((p)); \
 while ((_z-- > (p)) && (*_z == ' ' || *_z == '\t' || *_z == '\n')) *_z = '\0';}

#define whitespace_newline_remove(p) {int n; \
  while ((*(p) == ' ') || (*(p) == '\t'))\
    (p)++; \
  n =strlen((p)) - 1; \
  if (p[n] == '\n') \
    *((p) + n) = '\0'; \
}

/* Object bit fields used for filtering
 * purposes.  A corresponding bit-field
 * set in database->obj_filter will
 * cause that object to be filtered out
 * on reloads, mirroring, and updates.
 */
enum OBJECT_FILTERS {
  XXX_F           = 0, /* this special filter value is given to all
			* non-key fields or to any field that should
			* never be filtered.
			*/
  AUT_NUM_F       = 01, 
  AS_SET_F        = 02,
  MNTNER_F        = 04,
  ROUTE_F         = 010,
  ROUTE6_F	  = 020,
  ROUTE_SET_F     = 040,
  INET_RTR_F      = 0100,
  RTR_SET_F       = 0200,
  PERSON_F        = 0400,
  ROLE_F          = 01000,
  FILTER_SET_F    = 02000,
  PEERING_SET_F   = 04000,
  KEY_CERT_F      = 010000,
  DICTIONARY_F    = 020000,
  REPOSITORY_F    = 040000,
  INETNUM_F       = 0100000,
  INET6NUM_F      = 0200000,
  AS_BLOCK_F      = 0400000,
  DOMAIN_F        = 01000000,
  LIMERICK_F      = 02000000,
  IPV6_SITE_F     = 04000000,
  /* NOTE: if you make any changes to this list
   * update 'o_filter' in config.c
   */
};
#define MAX_OBJECT_FILTERS 22
#define ROUTING_REGISTRY_OBJECTS (AUT_NUM_F|INET_RTR_F|MNTNER_F|ROUTE_F|ROUTE6_F|AS_SET_F|ROUTE_SET_F|FILTER_SET_F|RTR_SET_F|PEERING_SET_F|KEY_CERT_F|REPOSITORY_F)

/* m_info [] in commands.c uses these values.  So any changes 
 * to this data struct should also be reflected in m_info [].
 * m_info[] is used in the !m command query lookup to identify 
 * the query object type via values in this data struct.
 *
 * obj_template [] in templates.c also uses these values.  For
 * the '-t' command (ie, object template flag/command) uses
 * this data struct as a pointer into the obj_template[] array
 * to identify the requested object template type.
 *
 * key_info [] in scan.c uses this data struct as an array index.
 * Function get_curr_f () returns values of this data type which
 * are the index into key_info [].
 *
 * m_info [], obj_template [] and key_info [] are all dependent on
 * this data struct and so changes need to be fully propagated.
 */ 
enum IRR_OBJECTS {
  AUT_NUM=0,	/* 0 */ /* this should be first for !man to be fast */
  AS_SET,       /* 1 */
  MNTNER,	/* 2 */
  ROUTE,	/* 3 */
  ROUTE6,	/* 4 */
  ROUTE_SET,    /* 5 */
  INET_RTR,	/* 6 */
  RTR_SET,      /* 7 */
  PERSON,       /* 8 */
  ROLE,		/* 9 */
  FILTER_SET,   /* 10 */
  PEERING_SET,  /* 11 */
  KEY_CERT,     /* 12 */
  DICTIONARY,   /* 13 */
  REPOSITORY,   /* 14 */
  INETNUM,	/* 15 */
  INET6NUM,     /* 16 */
  AS_BLOCK,	/* 17 */
  DOMAIN,       /* 18 */
  LIMERICK,     /* 19 */
  IPV6_SITE,    /* 20 */
/* begin non-class attributes */
  ORIGIN,	/* 21 */
  MNT_BY,	/* 22 */
  ADMIN_C,      /* 23 */
  TECH_C,       /* 24 */
  NIC_HDL,      /* 25 */
  MBRS_BY_REF,	/* 26 */
  MEMBER_OF,	/* 27 */
  MEMBERS,	/* 28 */
  MP_MEMBERS,	/* 29 */
  WITHDRAWN,	/* 30 */
  SYNTAX_ERR,   /* 31 */
  WARNING,      /* 32 */
  PREFIX,	/* 33 */
  CONTACT,      /* 34 */
  AUTH,         /* 35 */
  NO_FIELD      /* 36 */
};
/* max keys should be one more than actual number */
#define IRR_MAX_CLASS_KEYS	21
#define IRR_MAX_KEYS	37

enum STATES {
  BLANK_LINE = 01, /* line starting with a '\n' */
  START_F = 02,  /* read the beginning of a line which fits in buffer */
  LINE_CONT = 04,  /* line continuation of previous line */
                   /* for overflow buffer lines, dump them */
  OVRFLW = 010,  /* read the beginning of a line which does not fit in buffer */
  OVRFLW_END = 020, /* we have finally encountered a '\n' for an OVRFLW line */
  DB_EOF = 040,     /* end of file, ie, nothing read */
  COMMENT = 0100    /* line starting with '#' */
};

enum F_PROPERTY { /* each field is either the class name field or non-class name field, also indicate if key-field for -K flag, and finally secondary index */
  NAME_F = 01,
  NON_NAME_F = 02,
  KEY_F = 04,
  SECONDARY_F = 010
};

enum SCAN_T { /* tell scan file to scan whole file or single object only */
  SCAN_FILE,
  SCAN_OBJECT
};

#endif /* SCAN_H */

