/* 
 * $Id: scan.c,v 1.26 2002/10/17 20:02:31 ljb Exp $
 * originally Id: scan.c,v 1.87 1998/07/29 21:15:17 gerald Exp 
 */

/* The routines in this file scan/parse the datase.db files */

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <fcntl.h>
#include <glib.h>

#include "mrt.h"
#include "trace.h"
#include "config_file.h"
#include "irrd.h"

/* local functions */
static int populate_keyhash (irr_database_t *database);
static void pick_off_secondary_fields (char *buffer, int curr_f, 
				       irr_object_t *irr_object);
void mark_deleted_irr_object (irr_database_t *database, u_long offset);
static void add_field_items (char *buf, LINKED_LIST **ll);
static char *build_indexes (FILE *fp, irr_database_t *db, irr_object_t *object, 
			    u_long fp_pos, int update_flag, char *first_attr);
int find_blank_line (FILE *fp, char *buf, int buf_size,
                  enum STATES state, enum STATES *p_save_state,
		  u_long *position, u_long *offset);
int dump_object_check (irr_object_t *object, enum STATES state, u_long mode, 
		       int update_flag, irr_database_t *db, FILE *fp);

/* Note: it is not necessary to define every field, only
 * those fields which irrd needs to recognize.
 *
 * Also note that the data struct 'enum IRR_OBJECTS' defined in
 * scan.h is the first dimension index into this array.  get_curr_f ()
 * returns the current field which acts as the index into this data struct.
 * Any changes to key_info [] or to the 'enum IRR_OBJECTS' data struct
 * need to be coordinated.  m_info [] and obj_template [] also depend
 * on 'enum IRR_OBJECTS'.  See scan.h for more information. */
key_label_t key_info [] = {
  {"aut-num:",      NAME_F|KEY_F, AUT_NUM_F},
  {"as-set:",       NAME_F|KEY_F, AS_SET_F},
  {"mntner:",       NAME_F|KEY_F, MNTNER_F},
  {"route:",        NAME_F|KEY_F, ROUTE_F},
  {"route6:",       NAME_F|KEY_F, ROUTE6_F},
  {"route-set:",    NAME_F|KEY_F, ROUTE_SET_F},
  {"inet-rtr:",     NAME_F|KEY_F, INET_RTR_F},
  {"rtr-set:",      NAME_F|KEY_F, RTR_SET_F},
  {"person:",       NAME_F|KEY_F, PERSON_F},
  {"role:",         NAME_F|KEY_F, ROLE_F},
  {"filter-set:",   NAME_F|KEY_F, FILTER_SET_F},
  {"peering-set:",  NAME_F|KEY_F, PEERING_SET_F},
  {"key-cert:",     NAME_F|KEY_F, KEY_CERT_F},
  {"dictionary:",   NAME_F, DICTIONARY_F},
  {"repository:",   NAME_F, REPOSITORY_F},
  {"inetnum:",      NAME_F|KEY_F, INETNUM_F},
  {"inet6num:",     NAME_F|KEY_F, INET6NUM_F},
  {"as-block:",     NAME_F|KEY_F, AS_BLOCK_F},
  {"domain:",       NAME_F, DOMAIN_F},
  {"limerick:",     NAME_F, LIMERICK_F},
  {"ipv6-site:",    NAME_F, IPV6_SITE_F},
/* beginning of non-class attribute names */
  {"origin:",       KEY_F|SECONDARY_F, XXX_F},
  {"mnt-by:",       SECONDARY_F, XXX_F},
  {"admin-c:",      NON_NAME_F, XXX_F},
  {"tech-c:",       NON_NAME_F, XXX_F},
  {"nic-hdl:",      KEY_F|SECONDARY_F, XXX_F},
  {"mbrs-by-ref:",  SECONDARY_F, XXX_F},
  {"member-of:",    SECONDARY_F, XXX_F},
  {"members:",      KEY_F|SECONDARY_F, XXX_F},
  {"mp-members:",   KEY_F|SECONDARY_F, XXX_F},
  {"withdrawn:",    SECONDARY_F, XXX_F},
  {"*error*:",      NON_NAME_F, XXX_F},
  {"warning:",      NON_NAME_F, XXX_F},
  {"prefix:",       SECONDARY_F, XXX_F},
  {"contact:",      NON_NAME_F, XXX_F},
  {"auth:",         NON_NAME_F, XXX_F},
  {"roa-status:",   NON_NAME_F, XXX_F},
  /* this should not change, add others before (ie, NO_FIELD row) */
  {"",      NON_NAME_F, XXX_F},
};

#define MAX_RPSLNAME_LEN 32

/* scan_irr_serial
 * read the <DB>.serial file and store the number for later use 
 */
/* JMH - The serial number file is important only for mirrors
 *       and for authoritative databases.  If its not there, starting
 *	 from zero is fine.  This function could happily be void,
 *	 except that we can use it as a existance test for the file
 *	 and that it contains a valid u_long
 */
int scan_irr_serial (irr_database_t *database) {
  char tmp[BUFSIZE], file[BUFSIZE];
  FILE *fp;
  uint32_t serial = 0;
  int ret_code = 0;

  strcpy (tmp, database->name);
  convert_toupper (tmp);

  sprintf (file, "%s/%s.CURRENTSERIAL", IRR.database_dir, tmp);
  fp = fopen (file, "r");

  if (fp != NULL) {
    memset (tmp, 0, sizeof (tmp));
    if (fgets (tmp, sizeof (tmp), fp) != NULL) {
      if (convert_to_32 (tmp, &serial) == 1) {
	database->serial_number = serial;
	ret_code = 1;
      }
      else
        database->serial_number = 0;
    }
    fclose (fp);
  }

  return ret_code;
}

/* Write to disk the current serial value for DB (db->name).
 *
 * Function was rewritten to return a value to indicate if
 * there were any errors in writing the serial value and also
 * to avoid buffering issues.  This is part of our support for 
 * atomic transactions.
 *
 * Note: Later we should write the current serial value as the
 * first line in the DB in a fixed field.  The original choice to
 * have a seperate current serial file was (IMHO) a poor choice.
 *
 * Input:
 *  -pointer to the DB struct so we know the name of the DB 
 *   and the current serial value to write to disk (db)
 *
 * Return:
 *  -1 if the current serial value was written to disk without error.
 *  -0 otherwise.
 */
int write_irr_serial (irr_database_t *db) {
  int ret_code = 0;
  char dbname[BUFSIZE], file[BUFSIZE], serial[20];
  int fd;

  /* make the current serial file name */
  strcpy (dbname, db->name);
  convert_toupper (dbname);
  sprintf (file, "%s/%s.CURRENTSERIAL", IRR.database_dir, dbname);

  /* now write the current serial to file */
  if ((fd = open (file, O_WRONLY|O_TRUNC|O_CREAT, 0644)) >= 0) {
    sprintf (serial, "%u", db->serial_number);
    if (write (fd, serial, strlen (serial)) > 0)  {
      SetStatusString (IRR.statusfile, dbname, "currentserial", serial);
      ret_code = 1;
    } else
      trace (ERROR, default_trace, "write_irr_serial (): file write error: (%s)\n",
	     strerror (errno));

    close (fd);
  }
  else
    trace (ERROR, default_trace, "write_irr_serial (): file open error: (%s)\n",
	   strerror (errno));

  return ret_code;
}

void write_irr_serial_export (uint32_t serial, irr_database_t *database) {
  char db[BUFSIZE], file[BUFSIZE], serial_out[20];
  FILE *fp;

  if (database->export_filename != NULL) 
    strcpy (db, database->export_filename);
  else
    strcpy (db, database->name);
  convert_toupper(db);

  sprintf (file, "%s/%s.CURRENTSERIAL", IRR.ftp_dir, db);
  if ((fp = fopen (file, "w")) != NULL) {
    sprintf (serial_out, "%u", serial);
    fwrite (serial_out, 1, strlen (serial_out), fp);
    fclose (fp);
  }
  SetStatusString (IRR.statusfile, db, "lastexport", serial_out);
}

/* scan_irr_file
 * Open the db, update, or mirror log file for parsing
 * The update_flag (if == 1 || 2) indicates this is a update/mirror file
 */
char *scan_irr_file (irr_database_t *database, char *extension, 
		     int update_flag, FILE *update_fp) {
  FILE *fp = update_fp;
  char file[BUFSIZE], *p;

  /* either an update or reading for the first time (a normal .db file) */
  if (update_flag)
    sprintf (file, "%s/.%s.%s", IRR.database_dir, database->name, extension);
  else {
    sprintf (file, "%s/%s.db", IRR.database_dir, database->name);
    fp = database->db_fp;
  }
  
  /* CL: open file */
  if (fp == NULL) {
     if ((fp = fopen (file, "r+")) == NULL) {
       /* try creating it for the first time */
       trace (NORM, default_trace, "scan.c (%s) does not exist, creating it\n", file);
        fp = fopen (file, "w+");
      
       if (fp == NULL) {
 	trace (ERROR, default_trace, "scan_irr_file () "
	       "Could not open %s (%s)!\n", 
	       file, strerror (errno));
	return "IRRd error: Disk error in scan_irr_file ().  Could not open DB";
      }
    }
    /* the following setvbuf consumes alot of memory, performance does not seem
       to be particularly improved by it.  comment out for now -LJB */
    /* setvbuf (fp, NULL, _IOFBF, IRRD_FAST_BUF_SIZE+2);*/ /* big buffer */
  }

  if (!update_flag) {
    database->db_fp = fp;
    /* rewind */
    if (fseek (database->db_fp, 0L, SEEK_SET) < 0) {
      trace (ERROR, default_trace, "scan_irr_file () rewind DB (%s) "
	     "error: %s\n", database->name, strerror (errno));
      return "scan_irr_file () rewind DB error.  Abort reload!";
    }
    database->time_loaded = time (NULL);
  }

  trace (NORM, default_trace, "Begin loading %s\n", file);

  /* open transaction journal */
  if (database->journal_fd < 0) {
    char jfile[BUFSIZE];

    make_journal_name (database->name, JOURNAL_NEW, jfile);
    
    if ((database->journal_fd = open (jfile, O_RDWR | O_APPEND| O_CREAT, 0664)) < 0)
      trace (ERROR, default_trace, "scan_irr_file () Could not open "
	     "journal file %s: (%s)!\n", jfile, strerror (errno));
  }
  
  /* if updating, log the serial number in the Journal file */
  if (update_flag)
    journal_maybe_rollover (database);

  if (IRR.key_string_hash == NULL)
    populate_keyhash (database);

  p = (char *) scan_irr_file_main (fp, database, update_flag, SCAN_FILE);

  fflush (database->db_fp);
  trace (NORM, default_trace, "Finished loading %s\n", file);
  return p;
}

/* scan_irr_file_main
 * Parse file looking for objects. 
 * update_flag tell's us if we are parsing a mirror file (2)
 * or !us...!ue update file (1), in contrast to a reload on bootstrap
 * This tells us to look for mirror file format errors
 * and save's us 'check for mirror header' cycles on reloads.
 */
void *scan_irr_file_main (FILE *fp, irr_database_t *database, 
                          int update_flag, enum SCAN_T scan_scope) {
  char buffer[4096], *cp, *p = NULL;
  char first_attr[128];
  u_long save_offset, offset, position, mode, len = 0;
  irr_object_t *irr_object;
  enum IRR_OBJECTS curr_f;
  enum STATES save_state, state;
  long lineno = 0;

  /* init everything */
  if (update_flag)
    position = save_offset = offset = (u_long) ftell (fp);
  else
    position = save_offset = offset = 0;

  mode       = IRR_NOMODE;
  state      = BLANK_LINE; 
  curr_f     = NO_FIELD;
  irr_object = NULL;

  if (scan_scope == SCAN_FILE)
    database->hash_spec_tmp = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, (GDestroyNotify)Delete_hash_spec);

  /* okay, here we go scanning the file */
  while (state != DB_EOF) { /* scan to end of file */
    if ((cp = fgets (buffer, sizeof (buffer), fp)) != NULL) {
      lineno++;
      position = offset;
      len = strlen(buffer);
      offset += len;
    }

    state = get_state (cp, len, state, &save_state);
    
    /* skip comment lines and lines that exceed the buffer size */
    if (state & (OVRFLW | OVRFLW_END | COMMENT ))
      continue;

    if (update_flag      &&
	state == START_F && 
	irr_object == NULL) {
      if (!strncmp ("%END", buffer, 4))
        break; /* normal exit from successful mirror */
      else {
	state = pick_off_mirror_hdr (fp, buffer, sizeof(buffer), state, &save_state,
	                             &mode, &position, &offset, database);
	/* something wrong with update, abort scan */
	if (state == DB_EOF) {
	  p = "IRRd error: scan_irr_file_main (): Missing 'ADD' or 'DEL' "
	      "and/or malformed update!";
	  break;
	}
      }
    }

    if (state & (DB_EOF | OVRFLW | OVRFLW_END | BLANK_LINE))
      curr_f = NO_FIELD;
    else if (state != LINE_CONT)
      curr_f = get_curr_f (buffer);  

    /* we have entered into a new object -- create the initial structure */
    if (irr_object == NULL && state == START_F) {
      irr_object = New_IRR_Object (buffer, position, mode);
      /* save a copy of the first attribute for logging purposes */
      strncpy(first_attr, buffer, sizeof(first_attr));
      first_attr[sizeof(first_attr) - 1] = '\0';
    }

    /* Ignore these fields if they come at the end of the object.
     * The trick is to treat the fields like a comment at the end of the
     * object.
     * dump_object_check() will set state = DB_EOF if other error's found 
     */
    if (curr_f == SYNTAX_ERR || curr_f == WARNING) {

      /* this junk should not be in a well-formed transaction */
      if (atomic_trans) {
	p = "IRRd error: scan_irr_file_main (): 'WARNING' or 'ERROR' "
	    "attribute found in update.  Abort transaction!";
	if (irr_object != NULL)
	  Delete_IRR_Object (irr_object);
	break;
      }

      trace (ERROR, default_trace,"In (db,serial)=(%s,%lu) found 'WARNING' "
	     "or '*ERROR*' line, attempting to remove extraneous lines starting with:\n%s", database->name, database->serial_number + 1, buffer);

      /* mark end of object, read past err's and warn's */ 
      save_offset = position; 

      state = find_blank_line (fp, buffer, sizeof(buffer), state, &save_state,
                               &position, &offset);
      state = dump_object_check (irr_object, state, mode, update_flag, 
                                 database, fp);
      if (state == DB_EOF) { /* something went wrong, dump object and abort */
          trace (ERROR, default_trace,"Attempt to remove RIPE server extraneous line failed!  Abort!\n");
        Delete_IRR_Object (irr_object);
        irr_object = NULL;
        mode = IRR_NOMODE;
        continue;
      } else
        trace (NORM, default_trace, "Attempt to remove extraneous line succeeded!\n");
    }

    if (curr_f != NO_FIELD && (state & (START_F | LINE_CONT)) ) {

      /* if continuation line, attribute value starts at beginning + 1 */
      if (state == LINE_CONT)
        cp = buffer + 1; /* Ignore initial whitespace or '+' */
      else /* skip over attribute name label */
	cp = buffer + strlen(key_info[curr_f].name);

      /* NAME_F indicates object class name attribute */
      if (key_info[curr_f].f_type & NAME_F) {
        whitespace_remove(cp);
	if (*cp != '\0') { /* class name value may be on a continuation line */
          if (irr_object->name != NULL) {
 	    /* Shouldn't have more than one class name attribute */
            trace (NORM, default_trace, "Warning! Multiple class name attributes: Previous - %s %s,  New - %s %s\n", key_info[irr_object->type].name, irr_object->name, key_info[curr_f].name, cp );
	  } else {
	    irr_object->name = strdup (cp);
	    irr_object->type = curr_f;
	    irr_object->filter_val = key_info[curr_f].filter_val;
	  }
	}
      } else if (key_info[curr_f].f_type & SECONDARY_F)
        /* add secondary keys, and store things like origin, nic-hdl, etc.  */
        pick_off_secondary_fields (cp, curr_f, irr_object);

      continue;
    }

    /* Process OBJECT  (we just read a '\n' on a line by itself or eof) */
    if ( (state & (BLANK_LINE | DB_EOF)) && irr_object != NULL) {
      if (scan_scope == SCAN_OBJECT)
	return (void *) irr_object;

      if (curr_f == SYNTAX_ERR || curr_f == WARNING)
	position = save_offset;
      else if (state == DB_EOF)
	position = offset;

      if ((p = build_indexes (fp, database, irr_object, position, update_flag, first_attr)) != NULL) {
        if (!update_flag || (update_flag == 1 && atomic_trans))
	  state = DB_EOF; /* abort scan, something wrong found in input file */
	else
	  p = NULL;	/* ignore errors if mirroring or non-atomic update */
      }

      Delete_IRR_Object (irr_object);
      irr_object = NULL;
      mode = IRR_NOMODE;
    }
  } /* while (state != DB_EOF) */

  /* only do on reload's and mirror updates */
  if (scan_scope == SCAN_FILE) {
    if (p == NULL) 
      commit_spec_hash (database); /* commit if no critical errors */
    g_hash_table_destroy(database->hash_spec_tmp);
  }
  return (void *) p;	/* return error string (if any) */
}

/* pick_off_secondary_fields
 * store some information like as_origin, communities,
 * and secondary indicie keys
 * Need some error checking added --ljb XXX
 */
void pick_off_secondary_fields (char *buffer, int curr_f, 
				irr_object_t *irr_object) {
  char *cp = buffer;
  char *tmpptr;
 
  switch (curr_f) {
  case ORIGIN:
    /* if origin already found, ignore continuation lines */
    if (irr_object->origin_found)
      break;
    whitespace_newline_remove(cp);
    cp += 2;
    /* Check for dots for now, can remove as asplain is standardized */
    if ( (tmpptr = strchr(cp,'.')) != NULL) {
      *tmpptr = 0;
      irr_object->origin = atoi(cp)*65536 + atoi(tmpptr + 1);
      *tmpptr = '.';
      irr_object->origin_found = 1;
    } else {
      if (convert_to_32(cp, &irr_object->origin) != 1) {
        irr_object->origin = 0; /* bogus value, need better handling */
      } else
        irr_object->origin_found = 1;
    }
    break;
  case NIC_HDL:
    whitespace_newline_remove(cp);
    irr_object->nic_hdl = strdup (cp);
    break;
  case PREFIX:	/* for IPv6 site objects */
    if (irr_object->ll_prefix == NULL) /* create list if it does not exist */
      irr_object->ll_prefix = LL_Create (LL_DestroyFunction, free, 0);
    whitespace_newline_remove(cp);
    LL_Add (irr_object->ll_prefix, strdup(cp));
    break;
  case MNT_BY:
    add_field_items (cp, &irr_object->ll_mnt_by);
    break;
  case MEMBER_OF:
  /* ROUTE, ROUTE6 and AUT-NUM member-of: scanning */
    add_field_items (cp, &irr_object->ll_mbr_of);
    break;
  case MEMBERS:
  case MP_MEMBERS:
  /* AS-SET, ROUTE-SET members: scanning */
    add_field_items (cp, &irr_object->ll_mbrs);
    break;
  case MBRS_BY_REF:
  /* AS-SET, ROUTE-SET mbrs-by-ref: scanning */
    add_field_items (cp, &irr_object->ll_mbr_by_ref);
    break;
  default:
    break;
  } 
}
  
void add_field_items (char *buf, LINKED_LIST **ll) {
  char *cp = buf, *last;

  if (*ll == NULL) /* create list if it does not exist */
    *ll = LL_Create (LL_DestroyFunction, free, 0);
  whitespace_newline_remove (cp);
  strtok_r (cp, ",", &last); 
  while (cp != NULL && *cp != '\0') {
    whitespace_remove (cp);
    /* sanity check */
    if (*cp != '\0') 
      LL_Add ((*ll), strdup (cp));
    
    cp = strtok_r (NULL, ",", &last);
  }
}

/* JW: note state OVRFLW needs to be fixed so that if it
 * occurs the object is dumped completely.  This is
 * consistent with transaction processing which will
 * come next.  The final step will be to roll-back
 * any objects that were added before the buffer
 * overflow and to report 'transaction abort' as
 * the return code.
 */

/* Given the previous state and the current line to be
 * processed return the next state which corresponds
 * to the current input line.  This state machine is
 * needed by the scan routine to know when it is processing
 * a new attribute so that it can build indexes, where the
 * end of the object is and to detect line continuation.
 * Several other routines also use this function for various
 * sub-tasks.  See scan.h for an explanation of the states.
 *
 * Return:
 *   state associated with the current input line
 * 
 */
int get_state (char *buf, u_long len, enum STATES state, enum STATES *p_save_state) {
  char *p;

  if (buf == NULL) return DB_EOF;

  if (buf[len - 1] != '\n') return OVRFLW;

  /*JW put in later to support buffer overflow processing
  if (state == OVRFLW) {
    if (buf[0] == '\n')     return BLANK_LINE;

    if (buf[0] == '\r' && 
	buf[1] == '\n')     return BLANK_LINE;

    return OVRFLW;
    }*/

  /* can have comments embedded in fields, ignore input after '#' */
  if ((p = strchr (buf, '#')) != NULL) {
    if (p == buf) {         /* line starts with '#', we have a comment line */
      if (state != COMMENT)
	*p_save_state = state; /* save state for when we come out of comment */
      return COMMENT;
    }
    *p = '\0';              /* effectively ignore input at '#' and beyond */
  }

  if (state == COMMENT)
    state = *p_save_state;     /* comment is over, restore old state */

  if (state & (BLANK_LINE | OVRFLW_END)) {
    if (buf[0] == '\n')     return BLANK_LINE;

    if (buf[0] == '\r' && 
	buf[1] == '\n')     return BLANK_LINE;

    /* JMH - these are not really valid in the BLANK_LINE state 
       Will the parser ever pass this in? */

    if (buf[0] == ' '  ||
        buf[0] == '\t' ||
	buf[0] == '+')      return OVRFLW_END;

    return START_F;
  }

  if (state & (START_F | LINE_CONT)) {
    if (buf[0] == '\n')     return BLANK_LINE;

    if (buf[0] == '\r' && 
	buf[1] == '\n')     return BLANK_LINE;

    if (buf[0] == ' '  ||
        buf[0] == '\t' ||
	buf[0] == '+')      return LINE_CONT;

    return START_F;
  }

  return OVRFLW_END;
}

/* get the current field and return its index (eg, 'mnt-by:' or 'source:') */
int get_curr_f (char *buf) {
  keystring_hash_t *keystring_item;
  char *src, *dst;
  char tmp[MAX_RPSLNAME_LEN + 1];
  int i;

  src = buf;
  dst = tmp;
  i = 0;

  while (i < MAX_RPSLNAME_LEN && *src != '\0') {
   if ( (*dst++ = tolower((int)*src++)) == ':') /* convert to lowercase and check for colon */
      break;
   i++;
  } 
  *dst = '\0';  /* write a null after the colon */

  keystring_item = g_hash_table_lookup(IRR.key_string_hash, tmp);

  if (keystring_item == NULL) {
    /* trace (NORM, default_trace,"Warning! Unrecognized attr: %s", buf); */
     return (NO_FIELD);
  }
  return (keystring_item->num);
}

/* read past an object to the first blank line */
int find_blank_line (FILE *fp, char *buf, int buf_size, 
                enum STATES state, enum STATES *p_save_state,
		u_long *position, u_long *offset) {
  char *cp;
  u_long len = 0;
  
  do {
    if ((cp = fgets (buf, buf_size, fp)) != NULL) {
      *position = *offset;
      len = strlen(buf);
      *offset += len;
    }
    state = get_state (cp, len, state, p_save_state);

    /* only dump lines if the ERROR or WARN line come 
     * at the end of the object.
     */
    if (state == START_F) {
      if (strncasecmp (key_info[WARNING].name, buf, 
		       strlen(key_info[WARNING].name)) &&
          strncasecmp (key_info[SYNTAX_ERR].name, buf, 
		       strlen(key_info[SYNTAX_ERR].name))) {
        trace (ERROR, default_trace,"find_blank_line(): Encountered ERROR or WARN line embedded within an object.  Abort scan at following line: %s", buf); 
        return (DB_EOF);
      }
    } 

  } while (state != BLANK_LINE && state != DB_EOF);

  return (state);
}

/* Checks for exactly one blank line past "ADD" or "DEL"
 * and then start of object.  Else something wrong, abort update, eg,
 * ADD
 * 
 * route: 198.108.60.0/24
 *
 */
int read_blank_line_input (FILE *fp, char *buf, int buf_size,
                           enum STATES state, enum STATES *p_save_state,
			   u_long *position, u_long *offset,
			   irr_database_t *database) {
  char *cp = NULL;
  int lineno = 0;
  u_long len = 0;
  
  do {
    if ((cp = fgets (buf, buf_size, fp)) != NULL) {
      *position = *offset;
      len = strlen(buf);
      *offset += len;
    }
    lineno++;
  } while ((state = get_state (cp, len, state, p_save_state)) == BLANK_LINE && lineno < 2);

  if (state == START_F && lineno == 2)
    return (state);
  else
    return (DB_EOF); /* something wrong with input, abort scan */
}

/* Check for an 'ADD' or 'DEL' at the begining of the
 * mirror or update.  Also calls read_blank_line_input ()
 * to check for exactly one blank line and then start
 * of object. Scans for protocol version 3 serial numbers
 * Version 3 may skip serials
 *
 * Return:
 *   START_F if no errors.
 *   DB_EOF otherwise.
 */
int pick_off_mirror_hdr (FILE *fp, char *buf, int buf_size,
                         enum STATES state, enum STATES *p_save_state,
			 u_long *mode, u_long *position,
			 u_long *offset, irr_database_t *db) {

  uint32_t serial_num;

  if (!strncasecmp ("ADD", buf, 3))  {
    *mode = IRR_UPDATE;
  }
  else if (!strncasecmp ("DEL", buf, 3))  {
    *mode = IRR_DELETE;
  }
  else
    state = DB_EOF; /* no "ADD" or "DEL" so abort scan */

  if (state != DB_EOF && db->mirror_protocol == 3) {
    if (convert_to_32(buf+4, &serial_num) != 1) {
      trace (ERROR, default_trace,"Serial num conversion error: %s\n", buf+4);
      state = DB_EOF;
    } else if (serial_num > db->serial_number) {
      db->serial_number = serial_num - 1; /* protocol 3 may skip serials */
    } else {
      trace (ERROR, default_trace,"Serial num error -- value unexpected, current value: %d\n", db->serial_number);
      state = DB_EOF;
    }
  }

  if (state != DB_EOF) {
    state = read_blank_line_input (fp, buf, buf_size, state, p_save_state, position, offset, db);
  } else {
    trace (ERROR, default_trace,"scan.c: pick_off_mirror_hdr(): abort scan\n");
    trace (ERROR, default_trace,"line (%s)\n", buf);
  }

  return (state);
}

char *build_indexes (FILE *fp, irr_database_t *db, irr_object_t *object, 
                     u_long fp_pos, int update_flag, char *first_attr) {
  u_long db_offset = 0;
  int del_obj = -1;
  int skip_obj = 0;
  char *p = NULL;

  /* good to have these checks so our scanner
     can deal with junk objects */

  if (update_flag && object->mode == IRR_NOMODE) {
    trace (ERROR, default_trace, "got object with no "
	   "ADD or DEL during udpate, obj key-(%s), db-(%s)\n", 
	   object->name, db->name); 
    return "Corrupted update file.  Missing 'ADD' or 'DEL'";   
  }

  if ( object->name == NULL ||     /* can get *XX: objects on db loads, */
       object->type == NO_FIELD) { /* these obj's have NULL names */
    if (update_flag) {       /* log unrecognized object classes on updates  */ 
      trace (NORM, default_trace, "Unknown object class - %s", first_attr); 
      p = "Unrecognized object class";
    }
    skip_obj = 1;
  }

  if (db->obj_filter & object->filter_val) {
     skip_obj = 1;	/* skip object if it should be filtered */
     /*  trace (NORM, default_trace, "Filtered object class - %s", first_attr);  */
  }

  object->len = fp_pos - object->offset;
  object->fp = fp;

  if (!skip_obj) {
    switch (object->mode) {
    case IRR_NOMODE:     /* reading .db file for first time */
      add_irr_object (db, object);
      break;

    case IRR_DELETE:
      if (!update_flag) { /* db file has a "DEL" in it, abort scan */
	trace (ERROR, default_trace, "DEL found in DB file, obj key-(%s)\n", object->name);
	return "DB file has a 'DEL'";
      }

      if ((del_obj = delete_irr_object (db, object, &db_offset)) > 0) {
	db->num_objects_deleted[object->type]++;
	db->num_changes++;
      }
      else if (update_flag == 1) { /* !us...!ue udpate = 1, mirror update = 2 */
	p = "Disk or indexing error! IRR_DELETE";
	trace (ERROR, default_trace, 
	       "DELETE disk or indexing error: key (%s)\n", object->name);
	return (p);
      }
      break;

    case IRR_UPDATE: /* implicit replace from mirror or update */
      if (!update_flag) { /* db file has an "ADD" in it, abort scan */
	trace (ERROR, default_trace,"ADD found in DB file, obj key-(%s)\n", object->name);
	return "DB file has a 'ADD'.  Abort!";
      }

      del_obj = delete_irr_object (db, object, &db_offset);
      if (add_irr_object (db, object) > 0) {
	db->num_objects_changed[object->type]++;
	db->num_changes++;
      }
      else {
	p = "Disk or indexing error! IRR_UPDATE: add_irr_object ()";
	trace (ERROR, default_trace, 
	       "UPDATE disk or indexing error: key (%s)\n", object->name);
      }
      break;

    default:
      trace (ERROR, default_trace,
	     "unrecognized mode (%d) obj key-(%s)\n", 
	     object->mode, object->name);
      return "Something wrong with file, no mode specified!";
    }
  }

  if (update_flag) {
    if (del_obj > 0)
      mark_deleted_irr_object (db, db_offset);
    if (!atomic_trans || update_flag != 1)
      journal_irr_update (db, object, object->mode, skip_obj);
  }

  return p;
}

/* An *ERROR* or WARNING line has been found, now see if the scan should
 * continue.  Also journal the update if scan continues.  We dump objects
 * with said fields and continue scanning if all these checks pass.
 */
int dump_object_check (irr_object_t *object, enum STATES state, u_long mode,
                       int update_flag, irr_database_t *database, FILE *fp) {
  
  if (mode == IRR_NOMODE) {
    if (update_flag)
      trace (ERROR, default_trace,"Missing 'ADD' or 'DEL' in mirror file "
	     "(%s), exit from dump object!\n", database->name);
    else
      trace (ERROR, default_trace,"'WARNING' or '*ERROR*' found in DB file "
	     "(%s), exit from dump object!\n", database->name);
    state = DB_EOF;
  }
  else if (!update_flag) {
    trace (ERROR, default_trace,"Serial update and 'WARNING' or '*ERROR*' "
	   "found in file (%s), exit from dump object!\n", database->name);
    state = DB_EOF;
  }
  else if (state == DB_EOF) { /* causes us to skip over bad obj, next mirror
                               * will be able to run */
    object->fp = fp;
    journal_irr_update (database, object, mode, 1);
  }

  return (state);
}

static int populate_keyhash (irr_database_t *database) {
  int i;

  IRR.key_string_hash = g_hash_table_new(g_str_hash, g_str_equal);

  /* populate */
    for (i = 0; i < IRR_MAX_KEYS; i++) {
      keystring_hash_t *keystring_item;

      if (strlen (key_info[i].name) <= 0) continue;
      keystring_item = irrd_malloc(sizeof(keystring_hash_t));
      keystring_item->key = strdup (key_info[i].name);
      keystring_item->num = i;
      g_hash_table_insert(IRR.key_string_hash, keystring_item->key, keystring_item);
    }
    return (1);
}

/* Get maxlen field from roa-status attribute */
int get_roamaxlen (char *buf, int *maxlen) {
  char *p, *last;

  if ( (p = strchr(buf, ':')) == NULL) /* advance to colon in attribute */
    return -1;
  p++;
  whitespace_remove(p);
  if (strncasecmp ("v=1", p, 3))
    return -1;	/* should be version 1 */
  strtok_r (p, ";", &last); 
  while (p != NULL && *p != '\0') {
    whitespace_remove (p);
    if (*p == 'm') {
      while (*p++ != '=' )  /* scan for equal sign */
	;
      whitespace_remove (p);
      *maxlen = atoi(p);
      return 0;
    }
    p = strtok_r (NULL, ";", &last);
  }
  return 0; /* no maxlen specified */
}
