
#ifndef _SCAN_H
#define _SCAN_H

#define whitespace_remove(p)  {char *_z; while (((p) != NULL) && (*(p) != '\0') \
                                       && ((*(p) == ' ') || (*(p) == '\t'))) \
				    (p)++; \
			      _z = (p) + strlen ((p)); \
                              while ((_z-- > (p)) && (*_z == ' ' || *_z == '\t' || *_z == '\n')) \
							*_z = '\0';}

#define whitespace_newline_remove(p) while (((p) != NULL) && (*(p) != '\0') \
					    && ((*(p) == ' ') || (*(p) == '\t'))) (p)++; \
				     *((p) + strlen ((p)) - 1) = '\0'
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
  AUTNUM_F        = 01, 
  ASMACRO_F       = 02,
  COMMUNITY_F     = 04,
  DOMAIN_F        = 010,
  INETNUM_F       = 020,
  INET6NUM_F      = 040,
  PERSON_F        = 0100,
  DOMAIN_PREFIX_F = 0200,
  INET_RTR_F      = 0400,
  LIMERICK_F      = 01000,
  MNTNER_F        = 02000,
  ROUTE_F         = 04000,
  ROLE_F          = 010000,
  IPV6_SITE_F     = 020000,
  AS_SET_F        = 040000,
  ROUTE_SET_F     = 0100000,
  FILTER_SET_F    = 0200000,
  RTR_SET_F       = 0400000,
  PEERING_SET_F   = 01000000,
  DICTIONARY_F    = 02000000,
  KEY_CERT_F      = 04000000,
  REPOSITORY_F    = 010000000,
  /* NOTE: if you make any changes to this list
   * update 'o_filter' in config.c
   */
};
#define MAX_OBJECT_FILTERS 23
#define ROUTING_REGISTRY_OBJECTS (AUTNUM_F|ASMACRO_F|COMMUNITY_F|MNTNER_F|ROUTE_F|AS_SET_F|ROUTE_SET_F|FILTER_SET_F|RTR_SET_F|PEERING_SET_F|KEY_CERT_F|REPOSITORY_F)

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
  ROUTE,	/* 1 */
  AS_MACRO,     /* 2 */
  COMMUNITY,	/* 3 */
  MAINT,	/* 4 */
  INET_RTR,	/* 5 */
  PERSON,       /* 6 */
  ROLE,		/* 7 */
  IPV6_SITE,	/* 8 */
  AS_LIST,	/* 9 */
  INETNUM,	/* 10 */
  ORIGIN,	/* 11 */
  COMM_LIST,	/* 12 */
  WITHDRAWN,	/* 13 */
  MNT_BY,	/* 14 */
  ADMIN_C,      /* 15 */
  TECH_C,       /* 16 */
  NIC_HDL,      /* 17 */
  DOMAIN,       /* 18 */
  SYNTAX_ERR,   /* 19 */
  WARNING,      /* 20 */
  MBRS_BY_REF,	/* 21 */
  MEMBER_OF,	/* 22 */
  MEMBERS,	/* 23 */
  PREFIX,	/* 24 */
  AS_SET,       /* 25 */
  RS_SET,       /* 26 */
  CONTACT,      /* 27 */
  FILTER_SET,   /* 28 */
  RTR_SET,      /* 29 */
  PEERING_SET,  /* 30 */
  KEY_CERT,     /* 31 */
  DICTIONARY,   /* 32 */
  REPOSITORY,   /* 33 */
  INET6NUM,     /* 34 */
  DOMAIN_PREFIX,/* 35 */
  NO_FIELD      /* 36 */
};
/* max keys should be one less than actual number */
#define IRR_MAX_KEYS	36

enum STATES {
  BLANK_LINE, /* line starting with a '\n' */
  START_F,    /* read the beginning of a line which fits in buffer */
  LINE_CONT,  /* line continuation of previous line */
              /* for overflow buffer lines, dump them */
  OVRFLW,     /* read the beginning of a line which does not fit in buffer */
  OVRFLW_END, /* we have finally encountered a '\n' for an OVRFLW line */
  DB_EOF,     /* end of file, ie, nothing read */
  COMMENT     /* line starting with '#' */
};

enum F_PROPERTY { /* each field is either a key field or non-key field */
  KEY_F,
  NON_KEY_F
};

enum SCAN_T { /* tell scan file to scan whole file or single object only */
  SCAN_FILE,
  SCAN_OBJECT
};

#endif /* SCAN_H */

