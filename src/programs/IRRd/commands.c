/*
 * $Id: commands.c,v 1.39 2002/10/17 20:02:29 ljb Exp $
 * originally Id: commands.c,v 1.101 1998/08/07 00:13:37 gerald Exp 
 */

#include <stdio.h>
#include <string.h>
#include "mrt.h"
#include "trace.h"
#include <time.h>
#include <signal.h>
#include "config_file.h"
#include <sys/types.h>
#include <ctype.h>
#include "radix.h"
#include "hash.h"
#include "stack.h"
#include <fcntl.h>
#include "irrd.h"
#include <errno.h>
#include <regex.h>

#define MAX_DB_NAME 64	/* max db name length */

extern trace_t *default_trace;
extern char *obj_template[];

/* m_info [] uses 'enum IRR_OBJECTS' in scan.h as the second
 * field in the struct.  This means changes to m_info []
 * or 'enum IRR_OBJECTS' need to be coordinated.  For example,
 * if a new object type is being added then a new entry should
 * be appended to this array and a new type added to 'enum IRR_OBJECTS'.
 *
 * key_info [] in scan.c and object_templates [] in templates.c 
 * also depend on 'enum IRR_OBJECTS' and so changes to any one
 * of these structs needs to be coordinated.
 */
m_command_t m_info [] = {
  { "an,", AUT_NUM },             /* 0 */
  { "aut-num,", AUT_NUM },        /* 1 */
  { "as-set,", AS_SET },          /* 2 */
  { "route-set,", ROUTE_SET },    /* 3 */
  { "mt,", MNTNER },              /* 4 */
  { "mntner,", MNTNER },          /* 5 */
  { "ir,", INET_RTR },            /* 6 */
  { "inet-rtr,", INET_RTR },      /* 7 */
  { "ro,", ROLE },                /* 8 */
  { "role,", ROLE },              /* 9 */
  { "pn,", PERSON },              /* 10 */
  { "person,", PERSON },          /* 11 */
  { "in,", INETNUM },             /* 12 */
  { "inetnum,", INETNUM },        /* 13 */
  { "rt,", ROUTE},                /* 14 */
  { "route,", ROUTE},             /* 15 */  
  { "route6,", ROUTE6},           /* 16 */  
  { "filter-set,", FILTER_SET},   /* 17 */
  { "rtr-set,", RTR_SET},         /* 18 */
  { "peering-set,", PEERING_SET}, /* 19 */
  { "key-cert,", KEY_CERT},       /* 20 */
  { "dictionary,", DICTIONARY},   /* 21 */
  { "repository,", REPOSITORY},   /* 22 */
  { "i6,", INET6NUM},             /* 23 */
  { "inet6num,", INET6NUM},       /* 24 */
  { "as-block,", AS_BLOCK},       /* 25 */
  { "domain,", DOMAIN},           /* 26 */
  { "dn,", DOMAIN},               /* 27 */
  { "limerick,", LIMERICK},       /* 28 */
  { "domain,", DOMAIN},           /* 29 */
  { "ipv6-site,", IPV6_SITE},     /* 30 */
/* defined in irrd.h
#define IRR_MAX_MCMDS           30
*/
};

/* Handler routines for IRR lookups
 * the show_xxx functions are human readable UII commands
 * the irr_xxx are used by the RAWhoisd machine telnet interface
 */

static void return_version (irr_connection_t *irr);
void irr_exact (irr_connection_t *irr, prefix_t *prefix, int flag, int mode);
void irr_less_all (irr_connection_t *irr, prefix_t *prefix, int flag, int mode);
void irr_show_sources (irr_connection_t *irr);
int irr_set_sources (irr_connection_t *irr, char *sources, int mode);
int irr_set_ALL_sources (irr_connection_t *irr, int mode); 
void irr_more_all (irr_connection_t *irr, prefix_t *prefix, int mode);
void irr_ripewhois(irr_connection_t *irr);
void irr_m_command (irr_connection_t *irr);
void irr_inversequery (irr_connection_t *irr, enum IRR_OBJECTS type, int mode, char *key);
void show_gas_answer (irr_connection_t *irr, char *key); 
void irr_journal_range (irr_connection_t *irr, char *db);
void irr_journal_add_answer (irr_connection_t *irr);

/* irr_process_command
 * read/parse the !command and call the appropriate handler
 */
void irr_process_command (irr_connection_t * irr) {
  char tmp[BUFSIZE];
  char *return_str = NULL;
  int update_done = 0, i, mode, n_updates, tmp_fd;
  
  if (irr->state == IRR_MODE_LOAD_UPDATE) {
    irr->begin_line = !irr->line_cont;
    i = strlen (irr->cp);
    irr->line_cont = (*(irr->cp + i - 1) != '\n');
    
    /* This section of code requires a bit of explanation.  The 
     * irr_read_command () function in telnet.c uses a fixed 
     * size memory buffer.  So to get around the problem of a 
     * !us...!ue line exceeding the buffer size, irr_read_command () 
     * may send an incomplete line (ie, a line *not* terminated by a 
     * '\n').  This means we need to detect line continuation so we 
     * can check for the update termination sequence "!ue\n" as 
     * "!ue\n" could occur on a boundry at the end of a read and 
     * the begining of the next read.  Finally, irr_read_command () 
     * will send lines terminate by a "\n" unless the line fills the 
     * buffer.  This simplifies the processing of the code below.
     */
    if (irr->begin_line) {
      irr->ue_test[0] = '\0';
      if (irr->line_cont) {
	if (i < 4) {
	  strcpy (irr->ue_test, irr->cp);
	  return;
	}
      }
      else if (!strncasecmp (irr->cp, "!ue", 3))
	update_done = 1;
    }
    else if (!irr->line_cont && i < 4) {
      i = strlen (irr->ue_test);
      strcat (irr->ue_test, irr->cp);
      if (!strncasecmp (irr->ue_test, "!ue", 3))
	update_done = 1;
      irr->ue_test[i] = '\0';
    }
    
    if (update_done) {
      
      trace (NORM, default_trace, "enter update done\n");
      
      fwrite ("\n%END\n", 1, 6, irr->update_fp);
      irr->state = 0;
      irr_update_lock (irr->database);
      
      /* rewind */
      if (fseek (irr->update_fp, 0L, SEEK_SET) < 0) {
	trace (ERROR, default_trace, "!ue file rewind positioning"
	       " error.\n");
	irr_update_unlock (irr->database);
	irr_send_error (irr, "ERROR: Transaction aborted!  "
			"!ue file rewind positioning error.");
	fclose (irr->update_fp);
	unlink (irr->update_file_name);
	return;
      }
      
      /* atomic transaction support */
      if (atomic_trans) {
	/* build the transaction file which can be used to 
	 * restore the DB to it's original state before the transaction */
	if ((return_str = build_transaction_file (irr->database, irr->update_fp, 
					 irr->update_file_name, tmp, 
					 &n_updates)) != NULL) {
	  trace (ERROR, default_trace, "Could not create transaction files: "
		 "%s\n", return_str);
	  irr_update_unlock (irr->database);
	  irr_send_error (irr, "ERROR: Transaction aborted!  "
			  "Could not build transaction files.");
	  fclose (irr->update_fp);
	  unlink (irr->update_file_name);
	  return;
	}
      }
      
      /* update the DB */
      return_str = scan_irr_file (irr->database, "update", 1, irr->update_fp);

      /* rollback the DB to it's original state if the transaction
       * could not be applied successfully in it's entirety */
      if (atomic_trans) {
	if (return_str != NULL) {
	  /* restore the DB to it's original state 
	   * then rebuild the indexes */
	  db_rollback (irr->database, tmp);
	  if (!irr_reload_database (irr->database->name, NULL, NULL))
	    trace (ERROR, default_trace, "DB rollback operation: index "
		   "rebuild failed for DB (%s)\n", irr->database->name);
	}
	/* update the *.JOURNAL file for authoritative or mirror DB's; 
	 * rpsdist will manage the journal in the other case */
	else if (((irr->database->flags & IRR_AUTHORITATIVE) ||
		 irr->database->mirror_prefix != NULL)            &&
		 !update_journal (irr->update_fp, irr->database, n_updates)) {
	  trace (ERROR, default_trace, "error in updating journal for DB (%s) "
		 "rolling back transaction\n", irr->database->name);
	  db_rollback (irr->database, tmp);
	  journal_rollback (irr->database, tmp);
	  return_str = "Transaction abort!  Internal journaling error.";
	}

	/* remove the transaction file */
	remove (tmp);
      }
      
      fclose (irr->update_fp);
      irr_update_unlock (irr->database);
      
      /* Send the client the transaction result */
      if (return_str == NULL) {
	irr_send_okay(irr);
	irr->database->last_update = time (NULL);
      }
      else
	irr_send_error (irr, return_str);
      
      /* remove the update file */
      remove (irr->update_file_name);
    }
    else { /* save the update to file */
      if (irr->ue_test[0] != '\0') {
	fwrite (irr->ue_test, 1, strlen (irr->ue_test), irr->update_fp);
	irr->ue_test[0] = '\0';
      }
      fwrite (irr->cp, 1, strlen (irr->cp), irr->update_fp);
    }
    
    return;
  }

  if (*irr->cp == '\0') {
    irr_write_nobuffer(irr, "% No search key specified\n\n");
    return;
  }
  
  trace (NORM, default_trace, "Command: %s\n", irr->cp);

  if (*irr->cp != '!')
    mode = RIPEWHOIS_MODE;
  else
    mode = RAWHOISD_MODE;

  if ( strlen(irr->cp) > IRR_MAXCMDLEN ) {
      irr_mode_send_error (irr, mode, "Command/Query exceeds max length!");
      return;
  }
  
  /* Process normal queries and any ripe flags.
   */
  if (mode == RIPEWHOIS_MODE) {
    irr_ripewhois(irr);
    /* need to clear flags */
    irr->ripe_flags = 0;
    return;      
  }

  if (!strncmp (irr->cp, "!!", 2)) {
    irr->stay_open = 1;
    return;
  }

  if (!strncmp (irr->cp, "!q", 2)) {
    irr->stay_open = 0;
    return;
  }

  /* Get routes with specified origin.   !gas1234 */
  if (!strncasecmp (irr->cp, "!gas", 4)) {
    char gas_key[BUFSIZE];

    irr->cp += 4;
    make_gas_key (gas_key, irr->cp);
    show_gas_answer (irr, gas_key);
    return;
  }

  /* rawhoisd match command !man,as1243 */
  if (!strncasecmp (irr->cp, "!m", 2)) {
    irr->cp += 2;
    irr_m_command (irr);
    return;
  }

  /* update !us<database> - DB we're authoritative for
   * ADD DEL object
   * terminated by a !ue
   *
   * We assume database files have already gone through irr_checker and
   * irr_auth. We return success or failure info to be used 
   * by irr_notify
   *
   */
  if ( !strncasecmp (irr->cp, "!us", 3) ){
    irr->cp += 3;
    irr->state = 0; /* reset state */
    
    /* see if we know of this DB */
    if ((irr->database = find_database (irr->cp)) == NULL) {
      irr_send_error (irr, "database not found");
      return;
    }
    
    /* check authoritative if !us*/
    if ( !(irr->database->flags & IRR_AUTHORITATIVE) ) {
      irr_send_error (irr, "cannot update non-authoritative database");
      return;
    }
    
    /* check security -- general */
    if ((irr->database->access_list > 0) &&
	(!apply_access_list (irr->database->access_list, irr->from))) {
      trace (NORM | INFO, default_trace, "Update access to %s denied for %s\n",
	     irr->database->name, prefix_toa (irr->from));
      irr_send_error (irr, "General access permission to denied");
      return;
    }
    
    /* 
     * check security -- write access 
     */
    if (irr->database->write_access_list == 0) {
      if (!prefix_is_loopback (irr->from)) {
	trace (NORM | INFO, default_trace, "No write access list sepcified -- Update access to %s denied for %s...\n",
	       irr->database->name, prefix_toa (irr->from));
	irr_send_error (irr, "update permission denied -- non-loopback");
	return;
      }
    }
    
    if ((irr->database->write_access_list > 0) &&
	(!apply_access_list (irr->database->write_access_list, irr->from))) {
      trace (NORM | INFO, default_trace, "Update access to %s denied for %s...\n",
	     irr->database->name, prefix_toa (irr->from));
      irr_send_error (irr, "update permission denied - failed access list");
      return;
    }
    
    trace (NORM, default_trace, "START update from %s to %s\n",
	   prefix_toa(irr->from), irr->database->name);
    
    sprintf (irr->update_file_name, "%s/%s.update.XXXXXX", 
	     IRR.database_dir, irr->database->name);

    /* create a file to dump the update to */
    if ((tmp_fd = mkstemp (irr->update_file_name)) < 0) {
      irr_send_error (irr, "IRRd error: initial !us file mkstemp () error");
      trace (ERROR, default_trace, "initial !us mkstemp () error\n");
      return;
    }
    
    if ((irr->update_fp = fdopen (tmp_fd, "r+")) == NULL) {
      /* open up the temp update file */
      snprintf (tmp, BUFSIZE, "IRRd error: initial !us file fdopen () error:"
		" %s\n", strerror (errno));
      irr_send_error (irr, tmp);
      trace (ERROR, default_trace, "%s", tmp);
      return;
    }
    
    irr->line_cont = 0;
    irr->ue_test[0] = '\0';
    irr->state = IRR_MODE_LOAD_UPDATE;
    irr_send_okay (irr);
    return;
  }
  
  /* withdrawn routes global flag */
  /* =1 -> show withdrawns's; =0 -> don't show */
  if (!strncmp (irr->cp, "!uwd", 4)) {
    irr->cp +=4;
    if (!strncmp (irr->cp, "=1", 2)) {
      irr->withdrawn = 1;
      irr_send_okay(irr);
    }
    else if (!strncmp (irr->cp, "=0", 2)) {
      irr->withdrawn = 0;
      irr_send_okay(irr);
    }
    else {
      irr_send_error (irr, NULL);
    }
    return;
  }
  
  /* full object flag */
  /* =1 -> show full object (default) ; =0 -> show <source> <key> only */
  /* affects the !r command only */
  if (!strncmp (irr->cp, "!ufo", 4)) {
    irr->cp +=4;
    if (!strncasecmp (irr->cp, "=1", 2)) {
      irr->full_obj = 1;
      irr_send_okay(irr);
    }
    else if (!strncmp (irr->cp, "=0", 2)) {
      irr->full_obj = 0;
      irr_send_okay(irr);
    }
    else {
      irr_send_error (irr, NULL);
    }
    return;
  }
  
  /* recurse or not */
  /* =1 -> no recursion; =0 -> recursion (default)*/
  if (!strncmp (irr->cp, "!uF", 3)) {
    irr->cp +=3;
    if (!strncmp (irr->cp, "=0", 2)) {
      irr->ripe_flags &= ~(FAST_OUT | RECURS_OFF);
      irr_send_okay(irr);
    }
    else if (!strncmp (irr->cp, "=1", 2)) {
      irr->ripe_flags |= FAST_OUT | RECURS_OFF;
      irr_send_okay(irr);
    }
    else
      irr_send_error (irr, NULL);
    
    return;
  }
  
  /* reload a database
   * !B<DB list ...> | ALL
   * !eg, !Brgnet,ripe 
   */
  if (!strncasecmp (irr->cp, "!B", 2)) {
    irr_database_t *database;
    char           *name, names[BUFSIZE], *last, *tmp_dir;
    
    irr->cp +=2;
    strcpy (names, irr->cp);
    if (!strcasecmp ("ALL", names)) {
      names[0] = '\0';
      LL_IntrIterate (IRR.ll_database, database) {
	strcat (names,",");
	strcat (names, database->name);
      }
    }

    /* expect the DB's to be in our irrd.conf defined tmp dir */
    tmp_dir = ((IRR.tmp_dir == NULL) ? "/var/tmp" : IRR.tmp_dir);
    name = strtok_r(names, ",", &last);
    while (name != NULL) {
      
      /* do we know about this DB? */
      if ((irr->database = find_database (name)) == NULL) {
	trace (ERROR, default_trace, "Database not found %s\n", name);
	irr_send_error (irr, NULL);
	break;
      }
      
      /* general access */
      if ((irr->database->access_list > 0) &&
          (!apply_access_list(irr->database->access_list, irr->from))) {
	trace (NORM | INFO, default_trace, "Reload of %s denied for %s...\n",
	       irr->database->name, prefix_toa(irr->from));
	irr_send_error (irr, NULL);
	break;
      }
      
      /* write access */
      if ((irr->database->write_access_list > 0) &&
	  (!apply_access_list(irr->database->write_access_list, irr->from))) {
	trace (NORM | INFO, default_trace, 
	       "Reload/write access to %s denied for %s...\n",
	       irr->database->name, prefix_toa(irr->from));
	irr_send_error (irr, NULL);
	break;
      }
      
      /* atomic reload */
      if (!irr_reload_database (name, NULL, tmp_dir))
	irr_send_error (irr, NULL);
      else
	irr_send_okay (irr);

      name = strtok_r(NULL, ",", &last);
    }
    return;
  }

  /* Get list of objects associated with a maintainer.  !omaint-as237 */
  if (!strncasecmp (irr->cp, "!o", 2)) {
    char maint_key[BUFSIZE];

    irr->cp += 2;
    irr->ll_answer = LL_Create (LL_DestroyFunction, free, 0);
    irr_lock_all (irr);
    make_mntobj_key (maint_key, irr->cp);
    irr_inversequery (irr, NO_FIELD, RAWHOISD_MODE, maint_key);
    return;
  }
  
  /* AS-SET/ROUTE-SET expansion !iAS-ESNETEU[,1] */
  if (!strncasecmp (irr->cp, "!i", 2)) {
    irr->cp += 2;
    irr_set_expand (irr, irr->cp);
    return;
  }
  
  /* Route searches.
   * Default finds exact prefix/len match.
   *  o - return origin of exact match(es)
   *  l - one-level less specific
   * eg, !r141.211.128/24,l
   */
  if (!strncasecmp (irr->cp, "!r", 2)) {
    char *cp = NULL;
    prefix_t *prefix;
    
    irr->cp += 2;
    cp = strrchr(irr->cp, ',');
    
    if (cp == NULL) {
      if ((prefix = ascii2prefix (0, irr->cp)) == NULL)
	irr_send_error (irr, NULL);
      else {
	irr_exact (irr, prefix, SHOW_FULL_OBJECT, RAWHOISD_MODE);
	Delete_Prefix (prefix);
      }
    }
    else if (!strncmp (",l", cp, 2)) {
      *cp = '\0';
      if ((prefix = ascii2prefix (0, irr->cp)) == NULL) {
	irr_send_error (irr, NULL);
      }
      else {
	irr_less_all (irr, prefix, SEARCH_ONE_LEVEL, RAWHOISD_MODE);
	Delete_Prefix (prefix);
      }
    }
    else if (!strncmp (",L", cp, 2)) {
      *cp = '\0';
      if ((prefix = ascii2prefix (0, irr->cp)) == NULL) {
	irr_send_error (irr, NULL);
      }
      else {
	irr_less_all (irr, prefix, SEARCH_ALL_LEVELS, RAWHOISD_MODE);
	Delete_Prefix (prefix);
      }
    }
    else if (!strncasecmp (",o", cp, 2)) {
      *cp = '\0';
      if ((prefix = ascii2prefix (0, irr->cp)) == NULL) {
	irr_send_error (irr, NULL);
      }
      else {
	irr_exact (irr, prefix, SHOW_JUST_ORIGIN, RAWHOISD_MODE);
	Delete_Prefix (prefix);
      }
    }
    else if (!strncasecmp (",M", cp, 2)) {
      *cp = '\0';
      if ((prefix = ascii2prefix (0, irr->cp)) == NULL) {
	irr_send_error (irr, NULL);
      }
      else {
	irr_more_all (irr, prefix, RAWHOISD_MODE);
	Delete_Prefix (prefix);
      }
    }
    else {
      /* error, unrecognized flag */
      irr_send_error (irr, "unrecognized flag");
      return;
    }
    return;
  }
  
  /* s command Set the sources to the specified list.
   * Default is all sources.	eg, !sradb,ans 
   */
  if (!strncasecmp (irr->cp, "!s", 2)) {
    irr->cp += 2;
    if (!strncasecmp (irr->cp, "-lc", 3))
      irr_show_sources (irr);
    else if (!strncasecmp (irr->cp, "-*", 2))
      irr_set_ALL_sources (irr, RAWHOISD_MODE); 
    else
      irr_set_sources (irr, irr->cp, RAWHOISD_MODE);
    return;
  }
  
  /* return a version */
  if ((!strncasecmp (irr->cp, "!ver", 4)) || 
      (!strcasecmp (irr->cp, "!v"))       || 
      (!strcasecmp (irr->cp, "!-v"))) {
    irr->cp += 5;
    return_version(irr);
    return;
  }
  
  /* set timeout, !t<seconds> (for programs like Roe, Aoe, etc */
  if (!strncasecmp (irr->cp, "!t", 2)) {
    irr->cp +=2;
    irr->timeout = atoi (irr->cp);
    
    if ((irr->timeout < 0) || (irr->timeout > 30*60)) {
      irr_send_error (irr, NULL);
      irr->timeout = 5*60;
    }
    else
      irr_send_okay (irr);
    return;
  }

  /* name of tool, !nroe  -- this should already be logged earlier */
  if (!strncasecmp (irr->cp, "!n", 2)) { 
    irr_send_okay (irr);
    return;
  }

  /* j command.  Show the journal ranges.  See irr_journal_range for
     more info.  -* is valid, just like !s, but is implemented in
     the function. */
  if (!strncasecmp (irr->cp, "!j", 2)) {
    irr_journal_range (irr, irr->cp+2);
    return;
  }

  /* error -- command unrecognized */
  irr_send_error(irr, "unrecognized command");
}

/* do an inverse lookup and return objects  */
void irr_inversequery (irr_connection_t *irr, enum IRR_OBJECTS obj_type, int mode,  char *key) {
  irr_database_t *db;
  hash_spec_t *hash_sval;
  objlist_t *obj_p;
  LINKED_LIST *ll;

  ll = LL_Create (LL_DestroyFunction, Delete_hash_spec, 0);

  LL_ContIterate (irr->ll_database, db) {
    if ((hash_sval = fetch_hash_spec (db, key, UNPACK)) != NULL) {
      LL_Iterate (hash_sval->ll_2, obj_p) {
        if (obj_type == NO_FIELD || obj_type == obj_p->type) 
	  irr_build_answer (irr, db, obj_p->type, obj_p->offset, 
				obj_p->len, NULL);
      }
      LL_Add (ll, hash_sval);
    }
  }

  send_dbobjs_answer (irr, DISK_INDEX, mode);
  irr_unlock_all (irr);
  irr_write_buffer_flush (irr);
  LL_Destroy (irr->ll_answer);
  LL_Destroy (ll);
}

/* !m... command, eg "!man,as1234" */
void irr_m_command (irr_connection_t *irr) {
  int found = 0, i;

  for (i = 0; i < IRR_MAX_MCMDS; i++) {
    if (!strncasecmp (irr->cp, m_info[i].command, strlen (m_info[i].command))) {
      found = 1;
      irr->cp += strlen (m_info[i].command);
      irr->ll_answer = LL_Create (LL_DestroyFunction, free, 0);

      irr_lock_all (irr);

      if (m_info[i].type == ROUTE || m_info[i].type == ROUTE6 || m_info[i].type == INET6NUM)
        lookup_prefix_exact (irr, irr->cp, m_info[i].type);
      else
        irr_database_find_matches (irr, irr->cp, PRIMARY, 
                                   RAWHOISD_MODE|TYPE_MODE, m_info[i].type, 
                                   NULL, NULL);
      break;
    }
  }

  if (found) {
    send_dbobjs_answer (irr, DISK_INDEX, RAWHOISD_MODE);
    irr_unlock_all (irr);
    irr_write_buffer_flush (irr);
    LL_Destroy (irr->ll_answer);
  } else  {
    irr_write_nobuffer (irr, "D\n");
  }

  return;
}

/* irr_ripewhois
 * Ripe whois commands, eg "whois AS2"
 */
void irr_ripewhois (irr_connection_t *irr) {
  prefix_t *prefix;
  int lookup_mode;
  char *key = irr->cp;
  char lookupkey[BUFSIZE];
  enum IRR_OBJECTS lookup_type;

  /* special mirror request command, e.g. -g RADB:1:232-LAST */
  if (!strncmp (key, "-g", 2)) {
    irr_service_mirror_request (irr, irr->cp+2);
    return;
  }

  /* special persistent connection command */
#ifdef NOTDEF
/* moved to parse_ripe_flags */
  if (!strncmp (key, "-k", 2)) {
    irr_write_nobuffer (irr, "\n"); /* IRRToolset wants a return */
    if (irr->stay_open == 0) {
      irr->stay_open = 1;
    } else
      irr->stay_open = 0;	/* if already persistent, close connection */
    return;
  }
#endif

  /* special "quit" keywords recognized by IRRd, note that RIPE software
     does not recognize these as special */
  if (!strcasecmp (key, "quit") ||
      !strcasecmp (key, "exit") ||
      !strcasecmp (key, "q")) { 
    irr->stay_open = 0;
    return;
  }

   /* clear flags to be consistent with RIPE server */
  irr->ripe_flags = 0;
  if (parse_ripe_flags (irr, &key) < 0)
    return;

  if (irr->ripe_flags & TEMPLATE) {
    if (irr->ripe_type >= 0 && irr->ripe_type < IRR_MAX_CLASS_KEYS)
      irr_write_nobuffer (irr, obj_template[irr->ripe_type]);
    return;
  }
  
  if ((irr->ripe_flags & SOURCES_ALL) &&
      (irr_set_ALL_sources (irr, RIPEWHOIS_MODE) == 0))
    return;

  if ((irr->ripe_flags & SET_SOURCE) && 
      (irr_set_sources (irr, irr->ripe_sources, RIPEWHOIS_MODE) == 0))
    return;

  irr_lock_all (irr);
  irr->ll_answer = LL_Create (LL_DestroyFunction, free, 0);

  if (*key >= '0' && *key <= '9' &&
      ( ((irr->ripe_flags & OBJ_TYPE) == 0) || irr->ripe_type == ROUTE || irr->ripe_type == ROUTE6 || irr->ripe_type == INET6NUM || irr->ripe_type == IPV6_SITE)) {
    /* let ascii2prefix figure out if it is an IPv4 or IPv6 address */
    prefix = ascii2prefix (0, key);

    if (prefix) {
      /* New RIPE stuff */
      if (irr->ripe_flags & LESS_ALL)
        irr_less_all (irr, prefix, SEARCH_ALL_LEVELS, RIPEWHOIS_MODE);
      else if (irr->ripe_flags & LESS_ONE)
        irr_less_all (irr, prefix, SEARCH_ONE_LEVEL_NOT_EXACT, RIPEWHOIS_MODE);
      else if (irr->ripe_flags & MORE_ALL) /* still need MORE_ONE support */
        irr_more_all (irr, prefix, RIPEWHOIS_MODE);
      else if (irr->ripe_flags & EXACT_MATCH)
	irr_exact(irr, prefix, SHOW_FULL_OBJECT, RIPEWHOIS_MODE);
      else
        irr_less_all (irr, prefix, SEARCH_ONE_LEVEL, RIPEWHOIS_MODE);
      Delete_Prefix (prefix);
    }
  }

  if (irr->ripe_flags & OBJ_TYPE)  {
    lookup_mode = TYPE_MODE;
    lookup_type = irr->ripe_type;
  }  else {
    lookup_mode = 0;
    lookup_type = NO_FIELD;
  }

  if ( irr->ripe_flags & INVERSE_ATTR ) {
    if (irr->inverse_type == MNT_BY) {  /* check for inverse mnt-by lookup */
      make_mntobj_key (lookupkey, key);
      irr_inversequery(irr, lookup_type, RIPEWHOIS_MODE, lookupkey); 
    } else if (irr->inverse_type == ORIGIN) { /* check for inverse origin lookup */
      if (!strncasecmp (key, "as", 2)) /* skip initial AS in string */
        key += 2;
      make_gas_key (lookupkey, key);
      irr_inversequery(irr, lookup_type, RIPEWHOIS_MODE, lookupkey); 
    } else if (irr->inverse_type == MEMBER_OF) { /* check for set membership */
      make_spec_key (lookupkey, NULL, key);
      irr_inversequery(irr, lookup_type, RIPEWHOIS_MODE, lookupkey); 
    } else {
      irr_unlock_all (irr);
      irr_write_buffer_flush (irr);
      LL_Destroy (irr->ll_answer);
    }
  } else {
    irr_database_find_matches (irr, key, PRIMARY, lookup_mode, lookup_type, NULL, NULL);

    if ((irr->ripe_flags & (FAST_OUT | RECURS_OFF | OBJ_TYPE | INVERSE_ATTR)) == 0)
      lookup_object_references (irr);  /* dupe ripe whois behavior */

    send_dbobjs_answer (irr, DISK_INDEX, RIPEWHOIS_MODE);
    irr_unlock_all (irr);
    irr_write_buffer_flush (irr);
    LL_Destroy (irr->ll_answer);
  }
}

void show_gas_answer (irr_connection_t *irr, char *key) {
  irr_database_t *db;
  int empty_answer = 1;
  hash_spec_t *hash_item;
  LINKED_LIST *ll;
  
  irr->ll_answer = LL_Create (LL_DestroyFunction, free, 0);
  ll = LL_Create (LL_DestroyFunction, Delete_hash_spec, 0);

  irr_lock_all (irr);
  LL_ContIterate (irr->ll_database, db) {
    if ((hash_item = fetch_hash_spec (db, key, FAST)) != NULL) {
      if (hash_item->len1 > 0) {
        if (!empty_answer) /* need to add a space between AS'es */
          irr_build_answer (irr, NULL, NO_FIELD, 0, 1, " ");
        irr_build_answer (irr, db, NO_FIELD, 0, 
			hash_item->len1 - 1, hash_item->gas_answer);
        empty_answer = 0;
      }
      LL_Add (ll, hash_item);
    }
  }

  /* tack on a carriage return */
  if (!empty_answer)
    irr_build_answer (irr, NULL, NO_FIELD, 0, 1, "\n");
  send_dbobjs_answer (irr, MEM_INDEX, RAWHOISD_MODE);
  irr_unlock_all (irr);
  irr_write_buffer_flush (irr);
  LL_Destroy (irr->ll_answer);
  LL_Destroy (ll);
} 


/* Route searches.  L -  all level less specific eg, !r141.211.128/24,L
   Route searches.  l - one-level less specific eg, !r141.211.128/24,l
   Both these searches are inclusive (ie, the supplied route will be 
   returned if it exits.
   The RIPE -l search is NOT inclusive and hence uses a different flag 
   flag=0 -> "l" search; flag=1 -> RIPE "-l" search; flag=2 -> "L" search */
void irr_less_all (irr_connection_t *irr, prefix_t *prefix, 
		   int flag, int mode) {
  radix_node_t *node = NULL;
  LINKED_LIST *ll_attr;
  irr_prefix_object_t *prefix_object;
  irr_answer_t *irr_answer;
  irr_database_t *database;
  prefix_t *tmp_prefix = NULL;
  int prefix_found = 0;

  if (mode == RAWHOISD_MODE) {
     irr->ll_answer = LL_Create (LL_DestroyFunction, free, 0);
     irr_lock_all (irr);
  }
	
  LL_ContIterate (irr->ll_database, database) {
    prefix_found = 0;

    tmp_prefix = prefix;

    /* first check if this node exists  */
    /* NOTE: do not show exact matches for RIPE -l searches */
    if ((node = prefix_search_exact (database, prefix)) != NULL && flag != SEARCH_ONE_LEVEL_NOT_EXACT) {
      ll_attr = (LINKED_LIST *) node->data;
      LL_Iterate (ll_attr, prefix_object) {
	if (mode == RIPEWHOIS_MODE ||
	    irr->withdrawn == 1    || 
	    prefix_object->withdrawn == 0) {
	  if (mode == RAWHOISD_MODE && irr->full_obj == 0)
	    irr_build_key_answer (irr, database->db_fp, database->name, prefix_object->type, prefix_object->offset, prefix_object->origin);
	  else
	    irr_build_answer (irr, database, prefix_object->type, prefix_object->offset, prefix_object->len, NULL);
	}
      }
    }

    if (flag == SEARCH_ONE_LEVEL && node != NULL)
       continue;	/* skip less specific if we got a match */

    /* now check all less specific */
    while ( (node = prefix_search_best (database, tmp_prefix)) != NULL) {
	tmp_prefix = node->prefix;

	ll_attr = (LINKED_LIST *) node->data;
	LL_Iterate (ll_attr, prefix_object) {
	  if (mode == RIPEWHOIS_MODE ||
	      irr->withdrawn == 1    || 
	      prefix_object->withdrawn == 0) {
	    prefix_found = 1;
	    if (mode == RAWHOISD_MODE && irr->full_obj == 0)
	      irr_build_key_answer (irr, database->db_fp, database->name, prefix_object->type, prefix_object->offset, prefix_object->origin);
	    else {
	      irr_build_answer (irr, database, prefix_object->type, prefix_object->offset, prefix_object->len, NULL);
            }
	  }
        }
      /* break out after one loop for ",l" and RIPE "-l" searches */
      if (prefix_found && (flag != SEARCH_ALL_LEVELS)) break; 
    }
  }
  
  if (mode != RAWHOISD_MODE) /* if using RIPE mode, data will be sent later */
    return;

  if (irr->full_obj == 0) {
    send_dbobjs_answer (irr, MEM_INDEX, RAWHOISD_MODE);
    LL_ContIterate (irr->ll_answer, irr_answer) {
      free (irr_answer->blob);
    }
  } else
    send_dbobjs_answer (irr, DISK_INDEX, RAWHOISD_MODE);
  irr_unlock_all (irr);
  irr_write_buffer_flush (irr);
  LL_Destroy(irr->ll_answer);
}

/* Route searches. M - all more specific eg, !r199.208.0.0/16,M */
/* TODO: Need to implement m for one level only more specific */
void irr_more_all (irr_connection_t *irr, prefix_t *prefix, int mode) {
  radix_node_t *node, *start_node, *last_node;
  LINKED_LIST *ll_attr;
  irr_prefix_object_t *prefix_object;
  radix_tree_t *radix;
  irr_answer_t *irr_answer;
  irr_database_t *database;
  
  if (prefix->bitlen < 8) {
    irr_mode_send_error (irr, mode, "only allow more specific searches >= /8");
    return;
  }

  if (mode == RAWHOISD_MODE) { /* ll_answer already created if using RIPE mode */
    irr->ll_answer = LL_Create (LL_DestroyFunction, free, 0);
    irr_lock_all (irr);
  }

  LL_ContIterate (irr->ll_database, database) {
    last_node = NULL;
    start_node = NULL;
    node = NULL;    

    if (prefix->family == AF_INET6)
      radix = database->radix_v6;
    else
      radix = database->radix_v4;

    /* memory  -- find the prefix, or the best large node */
    start_node = radix_search_exact_raw (radix, prefix);

    if (start_node != NULL) {
      RADIX_WALK (start_node, node) {
	trace (TRACE, default_trace, "  -M %s\n", prefix_toax (node->prefix));
	if ((node->prefix != NULL) &&
	    (node->prefix->bitlen > prefix->bitlen) &&
	    (comp_with_mask ((void *) prefix_tochar (node->prefix), 
			     (void *) prefix_tochar (prefix),  prefix->bitlen))) {
	  ll_attr = (LINKED_LIST *) node->data;
	  LL_Iterate (ll_attr, prefix_object) {
	    if ((irr->withdrawn == 1) || (prefix_object->withdrawn == 0)) {
	      if (mode == RAWHOISD_MODE && irr->full_obj == 0)
		irr_build_key_answer (irr, database->db_fp, database->name, prefix_object->type, prefix_object->offset, prefix_object->origin);
	      else
		irr_build_answer (irr, database, prefix_object->type, prefix_object->offset, prefix_object->len, NULL);
	    }
	  }
	}
      }
      RADIX_WALK_END;
    }
  }
  
  if (mode != RAWHOISD_MODE)  /* if using RIPE MODE, data will be sent later*/
    return;

  if (irr->full_obj == 0) {
    send_dbobjs_answer (irr, MEM_INDEX, RAWHOISD_MODE);
    LL_ContIterate (irr->ll_answer, irr_answer) {
      free (irr_answer->blob);
    }
  } else
    send_dbobjs_answer (irr, DISK_INDEX, RAWHOISD_MODE);
  irr_unlock_all (irr);
  irr_write_buffer_flush (irr);
  LL_Destroy(irr->ll_answer);
}

/* Route searches.  o - return origin of exact match(es) eg, !r141.211.128/24,o 
 * flag == 1 means just return the ASorigin. If not 1, return the full object 
 */
void irr_exact (irr_connection_t *irr, prefix_t *prefix, int flag, int mode) {
  radix_node_t *node = NULL;
  LINKED_LIST *ll_attr;
  irr_prefix_object_t *prefix_object;
  irr_database_t *database;
  irr_answer_t *irr_answer;
  int first = 1;

  if (mode == RAWHOISD_MODE) {
    if (flag == SHOW_FULL_OBJECT)
      irr->ll_answer = LL_Create (LL_DestroyFunction, free, 0);
    irr_lock_all (irr);
  }

  LL_ContIterate (irr->ll_database, database) {

    node = prefix_search_exact (database, prefix);

    if (node != NULL) { 
      ll_attr = (LINKED_LIST *) node->data;
      LL_Iterate (ll_attr, prefix_object) {
	if (irr->withdrawn == 1 || prefix_object->withdrawn == 0) {
	  if (flag == SHOW_JUST_ORIGIN) {
	    if (mode == RAWHOISD_MODE && irr->full_obj == 0) {
	      if (first != 1) irr_add_answer (irr, "\n");
	      first = 0;
	      irr_add_answer (irr, "%s AS%d", database->name, prefix_object->origin);
	    } else {
	      irr_add_answer (irr, "AS%d ", prefix_object->origin);
	    }
	  } else {
	    if (mode == RAWHOISD_MODE && irr->full_obj == 0)
	      irr_build_key_answer (irr, database->db_fp, database->name, prefix_object->type, prefix_object->offset, prefix_object->origin);
	    else 
	      irr_build_answer (irr, database, prefix_object->type, prefix_object->offset, prefix_object->len, NULL);
	  }
	}
      }
    }
  }

  if (mode != RAWHOISD_MODE) /* if using RIPE mode, data will be sent later */
    return;

  if (flag == SHOW_JUST_ORIGIN) {
    irr_send_answer (irr);
  } else {
    if (irr->full_obj == 1)
      send_dbobjs_answer (irr, DISK_INDEX, RAWHOISD_MODE);
    else {
      send_dbobjs_answer (irr, MEM_INDEX, RAWHOISD_MODE);
      LL_ContIterate (irr->ll_answer, irr_answer) {
	free (irr_answer->blob);
      }
    }
  }   

  irr_unlock_all (irr);
  irr_write_buffer_flush (irr);
  if (flag == SHOW_FULL_OBJECT)
    LL_Destroy(irr->ll_answer);
}

/* s command Set the sources to the specified list.
 * eg, !sradb,ans 
 *
 * RETURN values:
 *
 * return number of sources set
 */

int irr_set_sources (irr_connection_t *irr, char *sources, int mode) {
  irr_database_t *database, *dup_db;
  int found = 0, dup;
  int ret_code = 0, old_ret_code;
  char tmp[BUFSIZE], buf[BUFSIZE], *last;
  int space_left = BUFSIZE - 1;
  char *db;

  buf[0] = buf[BUFSIZE - 1] = '\0';
  db = strtok_r(sources, ",", &last);
 
  while (db != NULL) {
    old_ret_code = ret_code;
    dup = 0;
    LL_IntrIterate (IRR.ll_database, database) {
      if (!strcasecmp (database->name, db)) {

	/* access-control  check */
	if ((database->access_list > 0) &&
	    (!apply_access_list (database->access_list, irr->from))) {
	  trace (NORM | INFO, default_trace, "Access to %s denied...\n",
		 prefix_toa (irr->from));
	  if (mode == RIPEWHOIS_MODE) {
            snprintf (tmp, BUFSIZE, "%%  Access denied for db source \"%s\".\n", database->name);
	    strncat (buf, tmp, space_left);
	    space_left = MAX((0), (space_left - strlen(tmp)));
          }
	}
	else {
	  /* hose the current list on our first DB match only */
	  if (!found) {
	    LL_Clear (irr->ll_database);
	    found = 1;
	  }

	  /* Don't add duplicate sources to the list */
	  LL_Iterate (irr->ll_database, dup_db) {
	    if (!strcmp (dup_db->name, database->name)) {
	      dup = 1;
	      break;
	    }
	  }

	  /* add database->name if it has not yet been added */
	  if (!dup) {
	    LL_Add (irr->ll_database, database);
	    ret_code++;
	  }
        }
        break;
      }
    }

    if (old_ret_code == ret_code       && 
	mode         == RIPEWHOIS_MODE) {
      if (dup)
	snprintf (tmp, BUFSIZE, "%%  Duplicate source \"%s\" ignored.\n", db);
      else
	snprintf (tmp, BUFSIZE, "%%  Source \"%s\" not found.\n", db);
      strncat (buf, tmp, space_left);
      space_left = MAX((0), (space_left - strlen(tmp)));
    }

    db = strtok_r(NULL, ",", &last);
  }

  if (mode == RAWHOISD_MODE) {
    if (ret_code == 0) 
      irr_send_error (irr, "source(s) unavailable");
    else
      irr_send_okay (irr);
  }
  else if (buf[0] != '\0') /* in RIPEWHOIS_MODE and errors, tell user */
    irr_write_nobuffer (irr, buf);

  return (ret_code);
}

/* Set db query list to all sources
 * ie, !s-*
 *
 * RETURN values:
 *
 * return number of sources set
 */
int irr_set_ALL_sources (irr_connection_t *irr, int mode) {
  irr_database_t *database;
  int ret_code = 0;
  char tmp[BUFSIZE], buf[BUFSIZE];

  buf[0] = '\0';

  LL_Clear (irr->ll_database);
  LL_IntrIterate (IRR.ll_database, database) {

    /* access-control  check */
    if ((database->access_list > 0) &&
	(!apply_access_list (database->access_list, irr->from))) {
      trace (NORM | INFO, default_trace, "Access to %s denied for %s...\n",
	     database->name, prefix_toa (irr->from));
      if (mode == RIPEWHOIS_MODE) {
        sprintf (tmp, "%%  Access denied for db source \"%s\".\n", database->name);
        strcat (buf, tmp);
      }
    }  
    else {
      LL_Add (irr->ll_database, database);
      ret_code++;
    }
  }

  if (mode == RAWHOISD_MODE) {
    if (ret_code == 0) 
      irr_send_error (irr, "access denied for db");
    else
      irr_send_okay (irr);
  }
  else if (buf[0] != '\0')  /* in RIPEWHOIS_MODE and errors, tell user */
      irr_write_nobuffer (irr, buf);

  return (ret_code);
}

/* !s-lc */
void irr_show_sources (irr_connection_t *irr) {
  irr_database_t *database;
  int first = 1;

  LL_Iterate (irr->ll_database, database) {
    if (first != 1) { irr_add_answer (irr, ",");}
    first = 0;
    irr_add_answer (irr, "%s", database->name);
  }
  irr_send_answer (irr);
}


/* Return a version string. eg, !ver */
static void return_version (irr_connection_t *irr) {
  irr_add_answer (irr, "# IRRd -- version %s ", IRRD_VERSION);
  irr_send_answer (irr);
}

/*
 * irr_journal_range
 * !jRADB,RIPE,FOO,BAR
 * !j-*
 * Use irr_add_answer/irr_sender answer to return:
 * A<n>
 * RADB:Y:1000-2000:1500
 * RIPE:N:0-666
 * FOO:X:<explanatory text - optional>
 * BAR:X:<explanatory text - optional>
 * C
 *
 * Y means that the database is mirrorable.
 * N means that the database is not mirrorable, but we're reporting
 *   the current serial number.  You can use this to check for updates.
 *   The first number will _always_ be zero.  The second number may be
 *   zero if the CURRENTSERIAL file doesn't exist.
 * X means that the database doesn't exist, or we're denying information
 *   about an existing database for administrative reasons.
 *
 * Returned DB's are canonicalized to upper case.
 */
void irr_journal_range (irr_connection_t *irr, char *db) {
  char *p_db, *last;

  p_db = strtok_r(db, ",", &last);

  /* Iterate through the list of dbs */
  while (p_db != NULL) {

    /* Special case, "-*" */
    if (!strncmp(p_db, "-*", 2)) {
      LL_IntrIterate (IRR.ll_database, irr->database) {
        irr_journal_add_answer(irr);
      }
    }
    else {
      /* Does the database exist? */
      if ((irr->database = find_database(p_db)) == NULL) {
	char db_canon[MAX_DB_NAME];

	strncpy (db_canon, p_db, MAX_DB_NAME - 1);
	db_canon[MAX_DB_NAME - 1] = '\0';  /* null terminate */
	convert_toupper(db_canon);

	trace (ERROR, default_trace, "Database not found %s\n", db_canon);
	irr_add_answer (irr, "%s:X:Database does not exist.\n", db_canon);
	p_db = strtok_r(NULL, ",", &last);
	continue;
      }

      irr_journal_add_answer(irr);
    }

    p_db = strtok_r(NULL, ",", &last);
  }

  irr_send_answer (irr);
}

void irr_journal_add_answer (irr_connection_t *irr) {
  enum { UNDETERMINED, DENIED, READONLY, MIRRORABLE } status = UNDETERMINED;
  char db_canon[MAX_DB_NAME], *p_last_export;
  u_long oldest_serial, current_serial, last_export = 0L;

  strncpy (db_canon, irr->database->name, MAX_DB_NAME - 1);
  db_canon[MAX_DB_NAME - 1] = '\0';
  convert_toupper(db_canon);

  /* If we're going to administratively deny access, do it here */

  /* Is it not authoritative or not a mirror? */
  if (!(((irr->database->flags & IRR_AUTHORITATIVE) == IRR_AUTHORITATIVE) ||
        (irr->database->mirror_prefix != NULL))) {
    status = READONLY;
  }
  else {
    status = MIRRORABLE;
  }

  /* We can set the db to be not-mirrorable even if it is for 
     administrative reasons.  This would be a good spot to check
     for ACLs on mirroring. */

  if ((irr->database->mirror_access_list > 0) && 
      !apply_access_list (irr->database->mirror_access_list, irr->from)) {
    status = READONLY;
    /* We don't really need to log the fact that they can't mirror the db.
       We're telling them they can't.  We'll log it if they request 
       mirroring. */
  }

  /* Fetch the lowest journal number */
  if (status == MIRRORABLE) {
    if (find_oldest_serial(irr->database->name, JOURNAL_OLD, &oldest_serial) != 1)
      if (find_oldest_serial(irr->database->name, JOURNAL_NEW, &oldest_serial) != 1)
	 oldest_serial = 0;
   }
   else {
     oldest_serial = 0;
   }
  
  /* Fetch the current serial number */
  current_serial = irr->database->serial_number;

  /* Get the last export from the StatusFile, if any */
  if ((p_last_export = GetStatusString(IRR.statusfile, db_canon, "lastexport")) != NULL) {
     last_export = atol(p_last_export);
  }

  if (last_export != 0L) {
    irr_add_answer(irr, "%s:%s:%lu-%lu:%lu\n", db_canon,
		   ( status == READONLY ) ? "N" : "Y",
		   oldest_serial, current_serial, last_export);
  }
  else {
    irr_add_answer(irr, "%s:%s:%lu-%lu\n", db_canon,
		   ( status == READONLY ) ? "N" : "Y",
		   oldest_serial, current_serial);
  }
}
