/* 
 * $Id: scan.c,v 1.22 2001/09/17 19:45:55 ljb Exp $
 * originally Id: scan.c,v 1.87 1998/07/29 21:15:17 gerald Exp 
 */

/* The routines in this file scan/parse the datase.db files */

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "mrt.h"
#include "trace.h"
#include <time.h>
#include <signal.h>
#include "config_file.h"
#include <fcntl.h>
#include "irrd.h"

extern trace_t *default_trace;

/* local functions */
static int populate_keyhash (irr_database_t *database);
static void pick_off_secondary_fields (char *buffer, int curr_f, 
                                       enum STATES state,
				       irr_object_t *irr_object,
				       enum DB_SYNTAX db_syntax);
void mark_deleted_irr_object (irr_database_t *database, u_long offset);
static void add_field_items (char *buf, int curr_f, enum STATES state, 
			     enum DB_SYNTAX db_syntax, LINKED_LIST **ll);
static char *build_indexes (FILE *fd, irr_database_t *db, irr_object_t *object, 
			    u_long fd_pos, int update_flag);
int find_blank_line (FILE *fd, char *buf, int buf_size,
                     enum STATES state, enum STATES *p_save_state,
		     u_long *position, u_long *offset,
		     enum DB_SYNTAX db_syntax, irr_database_t *db);
int dump_object_check (irr_object_t *object, enum STATES state, u_long mode, 
		       int update_flag, irr_database_t *db, FILE *fd);
void database_store_stats (irr_database_t *database);

/* Note: it is not necessary to define every field, only
 * those fields which irrd needs to recognize.
 * col 1 is RIPE181 and col 2 is RPSL 
 *
 * Also note that the data struct 'enum IRR_OBJECTS' defined in
 * scan.h is the first dimension index into this array.  get_curr_f ()
 * returns the current field which acts as the index into this data struct.
 * Any changes to key_info [] or to the 'enum IRR_OBJECTS' data struct
 * need to be coordinated.  m_info [] and obj_template [] also depend
 * on 'enum IRR_OBJECTS'.  See scan.h for more information. */
key_label_t key_info [] [2] = {
  { { "*an:", 4, KEY_F, AUTNUM_F},   { "aut-num:",      8, KEY_F, AUTNUM_F} },
  { { "*rt:", 4, KEY_F, ROUTE_F},    { "route:",        6, KEY_F, ROUTE_F} },
  { { "*am:", 4, KEY_F, ASMACRO_F},  { "",              1, KEY_F, ASMACRO_F} },
  { { "*cm:", 4, KEY_F, COMMUNITY_F},{ "",              1, KEY_F, COMMUNITY_F} },
  { { "*mt:", 4, KEY_F, MNTNER_F},   { "mntner:",       7, KEY_F, MNTNER_F} },
  { { "*ir:", 4, KEY_F, INET_RTR_F}, { "inet-rtr:",     9, KEY_F, INET_RTR_F} },
  { { "*pn:", 4, KEY_F, PERSON_F},   { "person:",       7, KEY_F, PERSON_F} },
  { { "*ro:", 4, KEY_F, ROLE_F},     { "role:",         5, KEY_F, ROLE_F} },
  { { "*is:", 4, KEY_F, IPV6_SITE_F},{ "ipv6-site:",   10, KEY_F, IPV6_SITE_F} },
  { { "*al:", 4, NON_KEY_F, XXX_F},  { "",              1, NON_KEY_F, XXX_F} },
  { { "*in:", 4, KEY_F, INETNUM_F},  { "inetnum:",      8, KEY_F,  INETNUM_F} },
  { { "*or:", 4, NON_KEY_F, XXX_F},  { "origin:",       7, NON_KEY_F, XXX_F} },
  { { "*cl:", 4, NON_KEY_F, XXX_F}, { "",              1, NON_KEY_F, XXX_F } },
  { { "*wd:", 4, NON_KEY_F, XXX_F}, { "withdrawn:",   10, NON_KEY_F, XXX_F } },
  { { "*mb:", 4, NON_KEY_F, XXX_F}, { "mnt-by:",       7, NON_KEY_F, XXX_F  } },
  { { "*ac:", 4, NON_KEY_F, XXX_F}, { "admin-c:",      8, NON_KEY_F, XXX_F } },
  { { "*tc:", 4, NON_KEY_F, XXX_F}, { "tech-c:",       7, NON_KEY_F, XXX_F } },
  { { "*nh:", 4, NON_KEY_F, XXX_F}, { "nic-hdl:",      8, NON_KEY_F, XXX_F} },
  { { "*dn:", 4, KEY_F, DOMAIN_F},  { "domain:",       7, KEY_F, DOMAIN_F } },
  { { "*ue:", 4, NON_KEY_F, XXX_F}, { "*ERROR*:",      8, NON_KEY_F, XXX_F } },
  { { "*uw:", 4, NON_KEY_F, XXX_F}, { "WARNING:",      8, NON_KEY_F , XXX_F} },
  /* rpsl specific fields */
  { { "",     1, NON_KEY_F, XXX_F}, { "mbrs-by-ref:", 12, NON_KEY_F, XXX_F } },
  { { "",     1, NON_KEY_F, XXX_F}, { "member-of:",   10, NON_KEY_F, XXX_F } },
  { { "",     1, NON_KEY_F, XXX_F}, { "members:",      8, NON_KEY_F, XXX_F } },
  { { "",     1, NON_KEY_F, XXX_F}, { "prefix:",       7, NON_KEY_F, XXX_F } },
  { { "",     1, NON_KEY_F, XXX_F}, { "as-set:",       7, KEY_F, AS_SET_F } },
  { { "",     1, NON_KEY_F , XXX_F},{ "route-set:",   10, KEY_F, ROUTE_SET_F } },
  { { "",     1, NON_KEY_F, XXX_F}, { "contact:",      8, NON_KEY_F, XXX_F } },
  { { "",     1, NON_KEY_F , XXX_F},{ "filter-set:",   11, KEY_F, FILTER_SET_F } },
  { { "",     1, NON_KEY_F , XXX_F},{ "rtr-set:",       8, KEY_F, RTR_SET_F } },
  { { "",     1, NON_KEY_F , XXX_F},{ "peering-set:",  12, KEY_F, PEERING_SET_F } },
  { { "",     1, NON_KEY_F , XXX_F},{ "key-cert:",      9, KEY_F, KEY_CERT_F } },
  { { "",     1, NON_KEY_F , XXX_F},{ "dictionary:",   11, KEY_F, DICTIONARY_F } },
  { { "",     1, NON_KEY_F , XXX_F},{ "repository:",   11, KEY_F, REPOSITORY_F } },
  { { "",     1, NON_KEY_F , XXX_F},{ "inet6num:",      9, KEY_F, INET6NUM_F } },
  { { "",     1, NON_KEY_F , XXX_F},{ "dom-prefix:",   11, KEY_F, DOMAIN_PREFIX_F } },
  /* this should not change, add others before (ie, NO_FIELD row) */
  { { "",1, NON_KEY_F, XXX_F }, { "",      1, NON_KEY_F, XXX_F } },
};

static char *str_syntax[] = {
	"RIPE181",
	"RPSL"
};

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
  FILE *fd;
  u_long serial = 0;
  int ret_code = 0;

  strcpy (tmp, database->name);
  convert_toupper (tmp);

  sprintf (file, "%s/%s.CURRENTSERIAL", IRR.database_dir, tmp);
  fd = fopen (file, "r");

  if (fd != NULL) {
    memset (tmp, 0, sizeof (tmp));
    if (fgets (tmp, sizeof (tmp), fd) != NULL) {
      if (convert_to_lu (tmp, &serial) == 1) {
	database->serial_number = serial;
	ret_code = 1;
      }
      else
        database->serial_number = 0;
    }
    fclose (fd);
  }

  return ret_code;
}

/* write_irr_serial
 * Write the serial number to <DB>.serial
 * we generally do this after mirroring and update our state
 */
int write_irr_serial_orig (irr_database_t *database) {
  int ret_code = 0;
  char db[BUFSIZE], file[BUFSIZE], serial[20];
  FILE *fd;

  strcpy (db, database->name);
  convert_toupper (db);

  sprintf (file, "%s/%s.CURRENTSERIAL", IRR.database_dir, db);
  if ((fd = fopen (file, "w")) != NULL) {
    sprintf (serial, "%ld", database->serial_number);
    fwrite (serial, 1, strlen (serial), fd);
    fclose (fd);
    ret_code = 1;
  }
  else
    trace (ERROR, default_trace, "write_irr_serial () file I/O open error: (%s)\n",
	   strerror (errno));

  SetStatusString (IRR.statusfile, db, "currentserial", serial);

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
  if ((fd = open (file, O_WRONLY|O_TRUNC, 0664)) >= 0) {
    sprintf (serial, "%ld", db->serial_number);
    if (write (fd, serial, strlen (serial)) > 0) 
      ret_code = 1;
    else
      trace (ERROR, default_trace, "write_irr_serial (): file I/O  error: (%s)\n",
	     strerror (errno));

    close (fd);
  }
  else
    trace (ERROR, default_trace, "write_irr_serial (): file I/O open error: (%s)\n",
	   strerror (errno));

  SetStatusString (IRR.statusfile, dbname, "currentserial", serial);

  return ret_code;
}

void write_irr_serial_export (u_long serial, irr_database_t *database) {
  char db[BUFSIZE], file[BUFSIZE], serial_out[20];
  FILE *fd;

  if (database->export_filename != NULL) 
    strcpy (db, database->export_filename);
  else
    strcpy (db, database->name);
  convert_toupper(db);

  sprintf (file, "%s/%s.CURRENTSERIAL", IRR.ftp_dir, db);
  if ((fd = fopen (file, "w")) != NULL) {
    sprintf (serial_out, "%ld", serial);
    fwrite (serial_out, 1, strlen (serial_out), fd);
    fclose (fd);
  }
  SetStatusString (IRR.statusfile, db, "lastexport", serial_out);
}

/* scan_irr_file
 * Open the db, update, or mirror log file for parsing
 * The update_flag (if == 1 || 2) indicates this is a update/mirror file
 * Determine the syntax (RIPE or RPSL)
 */
char *scan_irr_file (irr_database_t *database, char *extension, 
		     int update_flag, FILE *update_fd) {
  enum DB_SYNTAX syntax;
  FILE *fd = update_fd;
  char file[BUFSIZE], *p;
#if (defined(USE_GDBM) || defined(USE_DB1))
  char file_spec[BUFSIZE];
#endif /* USE_GDBM || USE_DB1 */

  /* either an update or reading for the first time (a normal .db file) */
  if (update_flag)
    sprintf (file, "%s/.%s.%s", IRR.database_dir, database->name, extension);
  else {
    sprintf (file, "%s/%s.db", IRR.database_dir, database->name);
    fd = database->fd;
#if (defined(USE_GDBM) || defined(USE_DB1))
    if (IRR.use_disk) {
      trace (TRACE, default_trace, 
	     "Calling irr_initialize_dbm_file () db->name (%s)\n", database->name);
      if ((database->dbm = irr_initialize_dbm_file (database->name)) == NULL) {
        trace (NORM, default_trace, 
	       "**** ERROR **** Could not open dbm file %s)!\n", file);
	trace (ERROR, default_trace, "Please check for file permissions and/or other disk/file problems.  Exit\n");
        return "Could not open indexing dbm file!";
      }
      sprintf (file_spec, "%s_spec", database->name);
      if ((database->dbm_spec = irr_initialize_dbm_file (file_spec)) == NULL) {
        trace (NORM, default_trace, 
	       "**** ERROR **** Could not open spec dbm file %s!\n", file);
        return "Could not open special indexing dbm file";
      }
    }
#endif /* USE_GDBM || USE_DB1 */
  }
  
  /* CL: open file */
  if (fd == NULL) {
     trace (NORM, default_trace, "scan.c opening file %s\n", file);
     if ((fd = fopen (file, "r+")) == NULL) {
       /* try creating it for the first time */
       trace (NORM, default_trace, "scan.c (%s) does not exist, creating it\n", file);
        fd = fopen (file, "w+");
      
       if (fd == NULL) {
 	trace (ERROR, default_trace, "IRRd error: scan_irr_file () "
	       "Could not open %s (%s)!\n", 
	       file, strerror (errno));
	return "IRRd error: Disk error in scan_irr_file ().  Could not open DB";
      }
    }
    /* the following setvbuf consumes alot of memory, performance does not seem
       to be particularly improved by it.  comment out for now -LJB */
    /* setvbuf (fd, NULL, _IOFBF, IRRD_FAST_BUF_SIZE+2);*/ /* big buffer */
  }

  if (!update_flag)
    database->fd = fd;

  if ((syntax = get_database_syntax (fd)) == UNRECOGNIZED) {
      trace (ERROR, default_trace, "Unrecognized DB syntax, reload aborted!\n");
      trace (ERROR, default_trace, "**** DB-(%s)\n", database->name);
      return "Unrecognized DB syntax!";
  }
  else if (syntax == EMPTY_DB) {
    trace (NORM, default_trace, "WARNING: No DB objects found, "
	   "scan complete DB-(%s)!\n", database->name);
    return "No DB objects found!";
  }
  
  if (database->db_syntax != syntax) {
    if (database->db_syntax == EMPTY_DB)
      database->db_syntax = syntax;
    else {
      trace (ERROR, default_trace, "DB syntax mismatch, reload aborted!\n");
      trace (NORM, default_trace, "**** (%s) is type (%s), update file type is"
	     " (%s).\n", database->name, str_syntax[database->db_syntax],
	     str_syntax[syntax]);
      return "DB syntax mismatch!";
    }
  }      

  if (IRR.database_syntax == EMPTY_DB)
    IRR.database_syntax = syntax;
  else if (IRR.database_syntax != MIXED &&
	   IRR.database_syntax != syntax)
    IRR.database_syntax = MIXED;

  if (!update_flag) {
    /* rewind */
    if (fseek (database->fd, 0L, SEEK_SET) < 0) {
      trace (ERROR, default_trace, "IRRd error: scan_irr_file () rewind DB (%s) "
	     "error: %s\n", database->name, strerror (errno));
      return "scan_irr_file () rewind DB error.  Abort reload!";
    }
    database->time_loaded = time (NULL);
  }

  trace (NORM, default_trace, "Starting Load %s\n", file);

  /* open transaction journal */
  if (database->journal_fd < 0) {
    make_journal_name (database->name, JOURNAL_NEW, file);
    
    if ((database->journal_fd = open (file, O_RDWR | O_APPEND| O_CREAT, 0774)) < 0)
      trace (ERROR, default_trace, "IRRd error: scan_irr_file () Could not open "
	     "journal file %s: (%s)!\n", file, strerror (errno));
  }
  
  /* if updating, log the serial number in the Journal file */
  if (update_flag)
    journal_maybe_rollover (database);

  if (IRR.key_string_hash == NULL)
    populate_keyhash (database);

  p = (char *) scan_irr_file_main (fd, database, update_flag, SCAN_FILE);

  fflush (database->fd);
  trace (NORM, default_trace, "Update/Mirror/Loading done...\n");
  return p;
}

/* scan_irr_file_main
 * Parse file looking for objects. 
 * update_flag tell's us if we are parsing a mirror file (2)
 * or !us...!ue update file (1), in contrast to a reload on bootstrap
 * This tells us to look for mirror file format errors
 * and save's us 'check for mirror header' cycles on reloads.
 */
void *scan_irr_file_main (FILE *fd, irr_database_t *database, 
                          int update_flag, enum SCAN_T scan_scope) {
  char buffer[4096], *cp, *p = NULL;
  u_long save_offset, offset, position, mode;
  irr_object_t *irr_object;
  enum IRR_OBJECTS curr_f;
  enum STATES prev_state, save_state, state;
  hash_spec_t hash_spec_item;
  long lineno = 0;

  /* init everything */
  if (update_flag)
    position = save_offset = offset = (u_long) ftell (fd);
  else
    position = save_offset = offset = 0;

  mode       = IRR_NOMODE;
  state      = BLANK_LINE; 
  curr_f     = NO_FIELD;
  irr_object = NULL;
  if (scan_scope == SCAN_FILE)
    database->hash_spec_tmp = 
      HASH_Create (1000, HASH_KeyOffset, 
		   HASH_Offset (&hash_spec_item, &hash_spec_item.key), 
		   HASH_DestroyFunction, Delete_hash_spec, 0);

  /* okay, here we go scanning the file */
  while (state != DB_EOF) { /* scan to end of file */

    /*if ((cp = buffer = my_fgets (database, fd)) != NULL) {*/
    if ((cp = fgets (buffer, sizeof (buffer), fd)) != NULL) {
      lineno++;
      position = offset;
      offset += strlen (buffer);
    }

    prev_state = state;
    state = get_state (buffer, cp, state, &save_state);
    
    /*JW This doesn'tallow #'s, +'s at the end of an object
    if (state == COMMENT &&
        prev_state != COMMENT)
      save_offset = position;
    JW*/
    /* dump comment lines and lines that exceed the buffer size */
    if (state == OVRFLW     || 
	state == OVRFLW_END ||
	state == COMMENT)
      continue;

    if (update_flag      &&
	state == START_F && 
	irr_object == NULL) {
      if (!strncmp ("%END", buffer, 4))
        break; /* normal exit from successful mirror */
      else {
	state = pick_off_mirror_hdr (fd, buffer, sizeof(buffer), state, &save_state,
	                             &mode, &position, &offset, database);
	/* something wrong with update, abort scan */
	if (state == DB_EOF) {
	  p = "IRRd error: scan_irr_file_main (): Missing 'ADD' or 'DEL' "
	      "and/or malformed update!";
	  break;
	}
      }
    }

    curr_f = get_curr_f (database->db_syntax, buffer, state, curr_f);  

    /* we have entered into a new object -- create the initial structure */
    if (irr_object == NULL    && 
	state      == START_F)
      irr_object = New_IRR_Object (buffer, position, mode);

    /* Ignore these fields if they come at the end of the object.
     * The trick is to treat the fields like a comment at the end of the
     * object.
     * dump_object_check() will set state = DB_EOF if other error's found 
     */
    if (curr_f == SYNTAX_ERR ||
        curr_f == WARNING) {

      /* this junk should not be in a well-formed transaction */
      if (atomic_trans) {
	p = "IRRd error: scan_irr_file_main (): 'WARNING' or 'ERROR' "
	    "attribute found in update.  Abort transaction!";
	if (irr_object != NULL)
	  Delete_IRR_Object (irr_object);
	break;
      }

      trace (ERROR, default_trace,"In (db,serial)=(%s,%lu) found 'WARNING' "
	     "or '*ERROR*' line, attempting to remove RIPE server extraneous lines:"
	     "\n%s", database->name, database->serial_number + 1, buffer);

      /* mark end of object, read past err's and warn's */ 
      save_offset = position; 

      state = find_blank_line (fd, buffer, sizeof(buffer), state, &save_state,
                               &position, &offset, database->db_syntax,
			       database);
      state = dump_object_check (irr_object, state, mode, update_flag, 
                                 database, fd);
      if (state == DB_EOF) { /* something went wrong, dump object and abort */
        trace (ERROR, default_trace,"Attempt to remove RIPE server extraneous line "
	       "failed!  Abort!\n");
        Delete_IRR_Object (irr_object);
        irr_object = NULL;
        mode = IRR_NOMODE;
        continue;
      }
      else
        trace (NORM, default_trace, "WARNING: Attempt to remove RIPE server "
	       "extraneous line succeeded!\n");
    }

    /* KEY_F type indicates a key field (ie, key field may not be first) */
    if (key_info[curr_f][database->db_syntax].f_type == KEY_F)
      init_key (buffer, curr_f, irr_object, database->db_syntax);
    
    /* add secondary keys, and store things like withdrawn,
     * origin, communities we'll need later
     */
    if (curr_f != NO_FIELD &&
	(state == START_F  || 
	 state == LINE_CONT)) {
      pick_off_secondary_fields (buffer, curr_f, state, irr_object, 
				 database->db_syntax);
      continue;
    }

    /* Process OBJECT  (we just read a '\n' on a line by itself or eof) */
    if ((state == BLANK_LINE || state == DB_EOF) && irr_object != NULL) {
      if (scan_scope == SCAN_OBJECT)
	return (void *) irr_object;

      if (curr_f == SYNTAX_ERR ||
	  curr_f == WARNING)
	position = save_offset;
      else if (state == DB_EOF)
	position = offset;

      if ((p = build_indexes (fd, database, irr_object, position, update_flag)) 
	  != NULL && (!update_flag || atomic_trans))
	state = DB_EOF; /* abort scan, something wrong found in input file */

      if (update_flag) {
	if (p == NULL)
	  commit_spec_hash (database); 
	HASH_Clear (database->hash_spec_tmp);
      }

      Delete_IRR_Object (irr_object);
      irr_object = NULL;
      mode = IRR_NOMODE;
    }
  } /* while (state != DB_EOF) */
  /*} BOGUS for % in vi */

  /* only do on reload's and mirror updates */
  if (scan_scope == SCAN_FILE) {
    database_store_stats (database);
    commit_spec_hash (database); 
    HASH_Destroy (database->hash_spec_tmp);
  }

  return (void *) p;
}

/* store stats in special indices
 * This is used so we can just load dbm files and not scan the .db file
 * to get all of this information
 */
void database_store_stats (irr_database_t *database) {
  char data[100], *key;

  /*
   * store database stats 
   * FORMAT:
   *   time last indexed #
   *   databse language (RPSL or RIPE) #
   *   number autnum #
   *   number route objects #
   */
  key = strdup ("@STATS");
  sprintf (data, "%d#%d#%d#%d#%d", 
	   (int) time(NULL), 
	   (int) database->db_syntax, 
	   database->bytes,
	   database->num_objects[ROUTE],
	   database->num_objects[AUT_NUM]);
  memory_hash_spec_store (database, key, DATABASE_STATS, NULL, NULL, data);
  Delete (key);
}

/* pick_off_secondary_fields
 * store some information like as_origin, communities,
 * and secondary indicie keys
 */
void pick_off_secondary_fields (char *buffer, int curr_f, 
                                enum STATES state, irr_object_t *irr_object,
				enum DB_SYNTAX db_syntax) {
  char *cp;
  
  /* rt object origin */
  if (curr_f == ORIGIN) {
    cp = buffer + key_info[ORIGIN][db_syntax].len;
    whitespace_newline_remove(cp);
    cp += 2;
    irr_object->origin = (u_short) atoi (cp);
  }

  else if (curr_f == NIC_HDL) {
    cp = buffer + key_info[NIC_HDL][db_syntax].len;
    whitespace_newline_remove(cp);
    irr_object->nic_hdl = strdup (cp);
  }
  
  /* rt object community list */
    else if (db_syntax == RIPE181 && curr_f == COMM_LIST) {

    cp = buffer + key_info[COMM_LIST][db_syntax].len;
    whitespace_newline_remove(cp);
    if (irr_object->ll_community == NULL) {
      irr_object->ll_community = LL_Create (LL_DestroyFunction, free, 0);
    }
    LL_Add (irr_object->ll_community, strdup (cp));
  }
  
  /* withdrawn */
  else if (curr_f == WITHDRAWN) {
    irr_object->withdrawn = 1;
  }
  
  /* prefix -- IPV6 Site object */
  else if (db_syntax == RPSL && curr_f == PREFIX) {

    cp = buffer + key_info[PREFIX][db_syntax].len;
    whitespace_newline_remove(cp);
    LL_Add (irr_object->ll_prefix, ascii2prefix (AF_INET6, cp));
  }
  
  /* ASMACRO *al: scanning, RIPE181 only */
  else if (db_syntax == RIPE181 && curr_f == AS_LIST)
    add_field_items (buffer, curr_f, state, db_syntax,
		     &irr_object->ll_as);

  /* AS-SET, RS-SET members: scanning, RPSL only */
  else if (db_syntax == RPSL && curr_f == MEMBERS)
    add_field_items (buffer, curr_f, state, db_syntax,
		     &irr_object->ll_as);

  /* AS-SET, RS-SET mbrs-by-ref: scanning, RPSL only */
  else if (db_syntax == RPSL && curr_f == MBRS_BY_REF) 
    add_field_items (buffer, curr_f, state, db_syntax,
		     &irr_object->ll_mbr_by_ref);

  /* ROUTE and AUT-NUM mnt-by: scanning, RPSL only */
  else if (db_syntax == RPSL && curr_f == MNT_BY)
    add_field_items (buffer, curr_f, state, db_syntax,
		     &irr_object->ll_mnt_by);

  /* ROUTE and AUT-NUM member-of: scanning, RPSL only */
  else if (db_syntax == RPSL && curr_f == MEMBER_OF)
    add_field_items (buffer, curr_f, state, db_syntax,
		     &irr_object->ll_mbr_of);
}
  
void add_field_items (char *buf, int curr_f, enum STATES state, 
		      enum DB_SYNTAX db_syntax, LINKED_LIST **ll) {
  char *cp, *last;
  /*  reference_key_t *ref_item; */

  cp = buf + key_info[curr_f][db_syntax].len;
  if (state == LINE_CONT)
    cp = buf;

  whitespace_newline_remove (cp);
  if (db_syntax == RPSL)
    strtok_r (cp, ",", &last); 
  else

    strtok_r (cp, " ", &last); 
  while (cp != NULL && *cp != '\0') {
    whitespace_remove (cp);

    if (*ll == NULL) /* this way we know if *ll != NULL list is non-empty */
      *ll = LL_Create (LL_DestroyFunction, free, 0);

    /* sanity check */
    if (strlen (cp) > 0) 
      LL_Add ((*ll), strdup (cp));
    
    if (db_syntax == RPSL)
      cp = strtok_r (NULL, ",", &last);
    else
      cp = strtok_r (NULL, " ", &last); 
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
int get_state (char *buf, char *cp, 
               enum STATES state, enum STATES *p_save_state) {
  char *p;

  if (cp == NULL) return DB_EOF;

  if (buf[strlen (buf) - 1] != '\n') return OVRFLW;

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

  if (state == BLANK_LINE || state == OVRFLW_END) {
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

  if (state == START_F || state == LINE_CONT) {
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

/* get the current field and return it (eg, 'mnt-by:' or 'source:') */
int get_curr_f (int db_syntax, char *buf, int state, int curr_f) {
  keystring_hash_t *keystring_item;
  char *cp, save = '\0';
  /*int i;*/

  if (state == LINE_CONT)
    return (curr_f);

  if (state == DB_EOF || 
      state == OVRFLW || 
      state == OVRFLW_END)
    return (NO_FIELD);

  /* do we need strncasecmp ????? */
  /* maybe use a hash to speed this up? */

  cp = strchr (buf, ':');
  if (cp != NULL) {
    cp++;
    save = *cp;
    *cp = '\0';
  }

  if ((keystring_item = HASH_Lookup (IRR.key_string_hash, buf)) == NULL) {
    if (cp != NULL) {*cp = save;}
    return (NO_FIELD);
  }

  if (cp != NULL) {*cp = save;}
  return (keystring_item->num);


  /*  for (i = 0; i < IRR_MAX_KEYS; i++) { 
    if (!memcmp (key_info[i][db_syntax].name, buf,  
		 key_info[i][db_syntax].len)) {
      return (i);
    }
  }

  return (NO_FIELD);*/
}

/* read past an object to the first blank line */
int find_blank_line (FILE *fd, char *buf, int buf_size, 
                     enum STATES state, enum STATES *p_save_state,
		     u_long *position, u_long *offset,
		     enum DB_SYNTAX db_syntax, irr_database_t *database) {
  char *cp;
  
  do {
    if ((cp = fgets (buf, buf_size, fd)) != NULL) {
      *position = *offset;
      *offset += strlen (buf);
    }
    /* } BOGUS for % in vi */

    state = get_state (buf, cp, state, p_save_state);

    /* only dump lines if the ERROR or WARN line come 
     * at the end of the object.
     */
    if (state == START_F) {
      if (strncmp (key_info[WARNING][db_syntax].name, buf, 
                  key_info[WARNING][db_syntax].len) &&
          strncmp (key_info[SYNTAX_ERR][db_syntax].name, buf, 
                  key_info[SYNTAX_ERR][db_syntax].len)) {
        trace (NORM, default_trace,"ERROR: find_blank_line(): Encountered ERROR or WARN line embedded within an object.  Line after embedded line:%s ie, could not read past.  Abort scan!\n", buf); 
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
int read_blank_line_input (FILE *fd, char *buf, int buf_size,
                           enum STATES state, enum STATES *p_save_state,
			   u_long *position, u_long *offset,
			   irr_database_t *database) {
  char *cp = NULL;
  int lineno = 0;
  
  do {
    /*if ((buf = my_fgets (database, fd)) != NULL) {*/
    if ((cp = fgets (buf, buf_size, fd)) != NULL) {
      *position = *offset;
      *offset += strlen (buf);
    }
    /* } BOGUS to make vi % happy */
    lineno++;
  } while ((state = get_state (buf, cp, state, p_save_state)) == BLANK_LINE && lineno < 2);

  if (state == START_F && lineno == 2)
    return (state);
  else
    return (DB_EOF); /* something wrong with input, abort scan */
}

/* Check for an 'ADD' or 'DEL' at the begining of the
 * mirror or update.  Also calls read_blank_line_input ()
 * to check for exactly one blank line and then start
 * of object.
 *
 * Return:
 *   START_F if no errors.
 *   DB_EOF otherwise.
 */
int pick_off_mirror_hdr (FILE *fd, char *buf, int buf_size,
                         enum STATES state, enum STATES *p_save_state,
			 u_long *mode, u_long *position,
			 u_long *offset, irr_database_t *db) {

  if (!strncasecmp ("ADD", buf, 3))  {
    *mode = IRR_UPDATE;
    state = read_blank_line_input (fd, buf, buf_size, state, p_save_state, position, offset, db);
  }
  else if (!strncasecmp ("DEL", buf, 3))  {
    *mode = IRR_DELETE;
    state = read_blank_line_input (fd, buf, buf_size, state, p_save_state, position, offset, db);
  }
  else
    state = DB_EOF; /* no "ADD" or "DEL" so abort scan */

  if (state == DB_EOF) {
    trace (ERROR, default_trace,"ERROR scan.c: pick_off_mirrro_hdr(): abort scan\n");
    trace (ERROR, default_trace,"ERROR line (%s)\n", buf);
  }

  return (state);
}

char *build_indexes (FILE *fd, irr_database_t *db, irr_object_t *object, 
                     u_long fd_pos, int update_flag) {
  u_long db_offset = 0;
  int del_obj = -1;
  char *p = NULL;

  /* good to have these checks so our scanner
     can deal with junk objects */
  if (object == NULL       ||
      object->name == NULL ||     /* can get *XX: objects on db loads, */
      object->type == NO_FIELD) { /* these obj's have NULL names */
    if (update_flag) {            /* so only abort on updates */ 
      trace (ERROR, default_trace, "IRRd error: build_indexes (): got a junk object, "
	     "update_flag-(%d) db-(%s)\n", update_flag, db->name); 
      return "IRRd error: build_indexes (): Unrecognized object!";
    }
    return NULL;
  }

  object->len = fd_pos - object->offset;
  object->fd = fd;

  if (!(db->obj_filter & object->filter_val)) {
    switch (object->mode) {
    case IRR_NOMODE:     /* reading .db file for first time */
      if (update_flag) { /* abort update scan, got object with no "ADD" or "DEL" */
	trace (ERROR, default_trace, "IRRd error: build_indexes(): Missing 'ADD' "
	       "or 'DEL': obj key-(%s), db-(%s)\n", object->name, db->name); 
	return "IRRd error: build_indexes() Corrupted update file.  "
	       "Missing 'ADD' or 'DEL'!"; 
      }
      add_irr_object (db, object);
      break;

    case IRR_DELETE:
      if (!update_flag) { /* db file has a "DEL" in it, abort scan */
	trace (ERROR, default_trace, "IRRd error: build_indexes (): DEL found "
	       "in db-(%s), obj key-(%s)\n", db->name, object->name);
	return "IRRd error: build_indexes (): DB file has a 'DEL'.  Abort!";
      }

      if ((del_obj = delete_irr_object (db, object, &db_offset)) > 0) {
	db->num_objects_deleted[object->type]++;
	db->num_changes++;
      }
      else if (update_flag == 1) { /* !us...!ue udpate = 1, mirror update = 2 */
	p = "Disk or indexing error! IRR_DELETE: delete_irr_object ()";
	trace (ERROR, default_trace, 
	       "ERROR: DELETE disk or indexing error: key (%s)\n", object->name);
	return (p);
      }
      break;

    case IRR_UPDATE: /* implicit replace from mirror or update */
      if (!update_flag) { /* db file has an "ADD" in it, abort scan */
	trace (ERROR, default_trace,"ERROR scan.c: build_indexes(): ADD found in db-(%s), obj key-(%s)\n", db->name, object->name);
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
	       "ERROR: UPDATE disk or indexing error: key (%s)\n", object->name);
      }
      break;

    default:
      trace (ERROR, default_trace,
	     "ERROR scan.c(build): unrecognized mode (%d) obj key-(%s) db-(%s)\n", 
	     object->mode, object->name, db->name);
      return "Something wrong with file, no mode specified!";
    }
  }
  else if (update_flag && object->mode == IRR_NOMODE) {
    trace (ERROR, default_trace, "IRRd error: build_indexes (): got object with no "
	   "ADD or DEL during udpate, obj key-(%s), db-(%s)\n", 
	   object->name, db->name); 
    return "IRRd error: build_indexes (): Corrupted update file.  Missing "
           " 'ADD' or 'DEL'!";   
  }
  

  if (update_flag) {
    if (del_obj > 0)
      mark_deleted_irr_object (db, db_offset);
    if (!atomic_trans)
      journal_irr_update (db, object, object->mode, db_offset);
  }

  return p;
}

/* An *ERROR* or WARNING line has been found, now see if the scan should
 * continue.  Also journal the update if scan continues.  We dump objects
 * with said fields and continue scanning if all these checks pass.
 */
int dump_object_check (irr_object_t *object, enum STATES state, u_long mode,
                       int update_flag, irr_database_t *database, FILE *fd) {
  
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
    object->fd = fd;
    journal_irr_update (database, object, mode, (u_long) 0);
  }

  return (state);
}


static int populate_keyhash (irr_database_t *database) {
  keystring_hash_t keystring_item;
  int i;

  IRR.key_string_hash =
    HASH_Create (100, HASH_KeyOffset, HASH_Offset (&keystring_item, &keystring_item.key), 0);

  /* populate */
    for (i = 0; i < IRR_MAX_KEYS; i++) {
      keystring_hash_t *keystring_item;

      if (strlen (key_info[i][database->db_syntax].name) <= 0) continue;
      keystring_item = New (keystring_hash_t);
      keystring_item->key = strdup (key_info[i][database->db_syntax].name);
      keystring_item->num = i;
      HASH_Insert (IRR.key_string_hash, keystring_item);
    }

    return (1);
}
