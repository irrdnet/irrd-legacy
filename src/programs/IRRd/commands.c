/*
 * $Id: commands.c,v 1.36 2001/08/07 17:09:55 ljb Exp $
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
#ifndef NT
#include <regex.h>
#endif /* NT */

extern trace_t *default_trace;
extern char *obj_template[];
char *p;

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
  { "am,", AS_MACRO },            /* 2 */
  { "as-macro,", AS_MACRO },      /* 3 */
  { "as-set,", AS_SET },          /* 4 */
  { "route-set,", RS_SET },       /* 5 */
  { "cm,", COMMUNITY },           /* 6 */
  { "community,", COMMUNITY },    /* 7 */
  { "mt,", MAINT },               /* 8 */
  { "mntner,", MAINT },           /* 9 */
  { "ir,", INET_RTR },            /* 10 */
  { "inet-rtr,", INET_RTR },      /* 11 */
  { "ro,", ROLE },                /* 12 */
  { "role,", ROLE },              /* 13 */
  { "pn,", PERSON },              /* 14 */
  { "person,", PERSON },          /* 15 */
  { "in,", INETNUM },             /* 16 */
  { "inetnum,", INETNUM },        /* 17 */
  { "rt,", ROUTE},                /* 18 */
  { "route,", ROUTE},             /* 19 */  
  { "filter-set,", FILTER_SET},   /* 20 */
  { "rtr-set,", RTR_SET},         /* 21 */
  { "peering-set,", PEERING_SET}, /* 22 */
  { "key-cert,", KEY_CERT},       /* 23 */
  { "dictionary,", DICTIONARY},   /* 24 */
  { "repository,", REPOSITORY},   /* 25 */
  { "i6,", INET6NUM},             /* 26 */
  { "inet6num,", INET6NUM},       /* 27 */
  { "dom-prefix,", DOMAIN_PREFIX},/* 28 */
  { "dp,", DOMAIN_PREFIX},        /* 29 */
  { "domain,", DOMAIN},           /* 30 */
  { "dn,", DOMAIN}                /* 31 */
/* defined in irrd.h
#define IRR_MAX_MCMDS           32 
*/
};

char *no_support_msg = "\n%% Command is disabled, object update abort!\n\n";

/* Handler routines for IRR lookups
 * the show_xxx functions are human readable UII commands
 * the irr_xxx are used by the RAWhoisd machine telnet interface
 */

static void return_version (irr_connection_t *irr);
void update_autnum_list (int expand_flag, LINKED_LIST *ll_macro_examined, 
			 LINKED_LIST *ll_aslist, LINKED_LIST *ll_as,
			 STACK **stack, char *db, irr_connection_t *irr);
char *macro_expand_add (char *name, irr_connection_t *irr, char *dbname);
void irr_exact (irr_connection_t *irr, prefix_t *prefix, int flag);
void irr_less_all (irr_connection_t *irr, prefix_t *prefix, 
		       int flag, int mode);
void irr_show_sources (irr_connection_t *irr);
int irr_set_sources (irr_connection_t *irr, char *sources, int mode);
int irr_set_ALL_sources (irr_connection_t *irr, int mode); 
void irr_more_all (irr_connection_t *irr, prefix_t *prefix, int mode);
void irr_origin (irr_connection_t *irr, char *origin);
void irr_ripewhois(irr_connection_t *irr, char * key);
void irr_m_command (irr_connection_t *irr);
void show_hash_spec_answer (irr_connection_t *irr, char *key); 
void irr_community (irr_connection_t *irr, char *name); 
void irr_journal_range (irr_connection_t *irr, char *db);
void irr_journal_add_answer (irr_connection_t *irr);

/* globals */
int begin_line, line_cont;
char ue_test[10];

/* irr_process_command
 * read/parse the !command and call the appropriate handler
 */
void irr_process_command (irr_connection_t * irr) {
  char tmp[BUFSIZE+1];
  int update_done = 0, i;
  int n_updates;
  int tmp_fd;
  
  if (irr->state == IRR_MODE_LOAD_UPDATE) {
    begin_line = !line_cont;
    i = strlen (irr->cp);
    line_cont = (*(irr->cp + i - 1) != '\n');
    
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
    if (begin_line) {
      ue_test[0] = '\0';
      if (line_cont) {
	if (i < 4) {
	  strcpy (ue_test, irr->cp);
	  return;
	}
      }
      else if (!strncasecmp (irr->cp, "!ue", 3))
	update_done = 1;
    }
    else if (line_cont == 0 && i < 4) {
      i = strlen (ue_test);
      strcat (ue_test, irr->cp);
      if (!strncasecmp (ue_test, "!ue", 3))
	update_done = 1;
      ue_test[i] = '\0';
    }
    
    if (update_done) {
      
      trace (NORM, default_trace, "enter update done\n");
      
      fwrite ("\n%END\n", 1, 6, irr->update_fd);
      irr->state = 0;
      irr_lock (irr->database);
      
      /* rewind */
      if (fseek (irr->update_fd, 0L, SEEK_SET) < 0) {
	trace (ERROR, default_trace, "IRRd ERROR: !ue file rewind positioning"
	       " error.\n");
	irr_unlock (irr->database);
	irr_send_error (irr, "IRRd ERROR: Transaction aborted!  "
			"!ue file rewind positioning error.");
	fclose (irr->update_fd);
	unlink (irr->update_file_name);
	return;
      }
      
      /* atomic transaction support */
      if (atomic_trans) {
	/* build the transaction file which can be used to 
	 * restore the DB to it's original state before the transaction */
	if ((p = build_transaction_file (irr->database, irr->update_fd, 
					 irr->update_file_name, tmp, 
					 &n_updates)) != NULL) {
	  trace (ERROR, default_trace, "Could not create transaction files: "
		 "%s\n", p);
	  irr_unlock (irr->database);
	  irr_send_error (irr, "IRRd ERROR: Transaction aborted!  "
			  "Could not build transaction files.");
	  fclose (irr->update_fd);
	  unlink (irr->update_file_name);
	  return;
	}
      }
      
      /* update the DB */
      trace (NORM, default_trace, "call scan_irr_file ()\n");
      p = scan_irr_file (irr->database, "update", 1, irr->update_fd);
      trace (NORM, default_trace, "after scan_irr_file ()\n");

      /* rollback the DB to it's original state if the transaction
       * could not be applied successfully in it's entirety */
      if (atomic_trans) {
	if (p != NULL) {
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
		 !update_journal (irr->update_fd, irr->database, n_updates)) {
	  trace (ERROR, default_trace, "error in updating journal for DB (%s) "
		 "rolling back transaction\n", irr->database->name);
	  db_rollback (irr->database, tmp);
	  journal_rollback (irr->database, tmp);
	  p = "Transaction abort!  Internal journaling error.";
	}

	/* remove the transaction file */
	remove (tmp);
      }
      
      fclose (irr->update_fd);
      irr_unlock (irr->database);
      
      trace (NORM, default_trace, "send client response...)\n");
      
      /* Send the client the transaction result */
      if (p == NULL) {
	irr_send_okay(irr);
	irr->database->last_update = time (NULL);
      }
      else
	irr_send_error (irr, p);
      trace (NORM, default_trace, "bye-bye update...)\n");
      
      /* remove the update file */
      remove (irr->update_file_name);
    }
    else { /* save the update to file */
      if (ue_test[0] != '\0') {
	fwrite (ue_test, 1, strlen (ue_test), irr->update_fd);
	ue_test[0] = '\0';
      }
      fwrite (irr->cp, 1, strlen (irr->cp), irr->update_fd);
    }
    
    return;
  }
  
  trace (NORM, default_trace, "Command: %s\n", irr->cp);
  
  if (irr->stay_open == 0 && 
      (!strncmp (irr->cp, "!!", 2) ||
       !strncmp (irr->cp, "-k", 2))) {
    irr->stay_open = 1;
    irr->short_fields = 1; /* this is what rawhoisd does */
    return;
  }
  
  /* quit or stay open */
  if (!strcasecmp (irr->cp, "quit") ||
      !strcasecmp (irr->cp, "exit") ||
      !strcasecmp (irr->cp, "!q")   ||
      !strcasecmp (irr->cp, "q")    ||
      !strcasecmp (irr->cp, "-k")) {
    irr->stay_open = 0;
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
      trace (NORM, default_trace, "ERROR -- Database not found %s\n",irr->cp);
      irr_send_error (irr, "Database not found!");
      return;
    }
    
    /* check authoritative if !us*/
    if ( !(irr->database->flags & IRR_AUTHORITATIVE) ) {
      trace (NORM, default_trace, 
	     "ERROR -- Request to update non-authoritative database %s with !us\n",irr->cp);
      irr_send_error (irr, "Request to update non-authoritative database!");
      return;
    }
    
    /* check security -- general */
    if ((irr->database->access_list > 0) &&
	(!apply_access_list (irr->database->access_list, irr->from))) {
      trace (NORM | INFO, default_trace, "Updated access to %s denied for %s...\n",
	     irr->database->name, prefix_toa (irr->from));
      irr_send_error (irr, "General access permission to denied!");
      return;
    }
    
    /* 
     * check security -- write access 
     */
    if (irr->database->write_access_list == 0) {
      if (!prefix_is_loopback (irr->from)) {
	trace (NORM | INFO, default_trace, "No write access list sepcified -- only loopback allowed\n");
	trace (NORM | INFO, default_trace, "Updated access to %s denied for %s...\n",
	       irr->database->name, prefix_toa (irr->from));
	irr_send_error (irr, "Write permission denied!");
	return;
      }
    }
    
    if ((irr->database->write_access_list > 0) &&
	(!apply_access_list (irr->database->write_access_list, irr->from))) {
      trace (NORM | INFO, default_trace, "Updated access to %s denied for %s...\n",
	     irr->database->name, prefix_toa (irr->from));
      irr_send_error (irr, "Write permission denied!");
      return;
    }
    
    trace (NORM, default_trace, "START update from %s to %s\n",
	   prefix_toa(irr->from), irr->database->name);
    
    sprintf (irr->update_file_name, "%s/%s.update.XXXXXX", 
	     IRR.database_dir, irr->database->name);

    /* create a file to dump the update to */
    if ((tmp_fd = mkstemp (irr->update_file_name)) < 0) {
      irr_send_error (irr, "IRRd error: initial !us file mkstemp () error");
      trace (ERROR, default_trace, "IRRd error: initial !us mkstemp () error\n");
      return;
    }
    
    if ((irr->update_fd = fdopen (tmp_fd, "r+")) == NULL) {
      /* open up the temp update file */
      snprintf (tmp, BUFSIZE, "IRRd error: initial !us file fdopen () error:"
		" %s\n", strerror (errno));
      irr_send_error (irr, tmp);
      trace (ERROR, default_trace, "%s", tmp);
      return;
    }
    
    line_cont = 0;
    ue_test[0] = '\0';
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
  
  
  /* long or short fields global flag */
  /* =1 -> short fields (fast!); =0 -> long fields (default)*/
  /* this will go away in rpsl because short fields are not used */
  if (!strncmp (irr->cp, "!uF", 3)) {
    irr->cp +=3;
    if (!strncmp (irr->cp, "=0", 2)) {
      irr->short_fields = 0;
      irr->ripe_flags &= ~(FAST_OUT | RECURS_OFF);
      irr_send_okay(irr);
    }
    else if (!strncmp (irr->cp, "=1", 2)) {
      irr->short_fields = 1;
      irr->ripe_flags |= FAST_OUT | RECURS_OFF;
      irr_send_okay(irr);
    }
    else
      irr_send_error (irr, NULL);
    
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
	trace (NORM, default_trace, "ERROR -- Database not found %s\n", name);
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
  
  /* Get routes with specified origin.   !gas1234 */
  if (!strncasecmp (irr->cp, "!gas", 4)) {
    irr->cp += 4;
    irr_origin (irr, irr->cp);
    
    return;
  }
  
  /* Get routes with specified community.  !hCOMM_NFSNET */
  if (!strncasecmp (irr->cp, "!h", 2)) {
    irr->cp += 2;
    irr_community (irr, irr->cp);
    return;
  }
  
  /* ASMacro expansion !iAS-ESNETEU[,1] */
  if (!strncasecmp (irr->cp, "!i", 2)) {
    irr->cp += 2;
    irr_as_set_expand (irr, irr->cp);
    return;
  }
  
  /* rawhoisd match command !man,as1243 */
  if (!strncasecmp (irr->cp, "!m", 2)) {
    irr->cp += 2;
    irr_m_command (irr);
    return;
  }
  
  /* return a version */
  if ((!strncasecmp (irr->cp, "!ver", 4)) || 
      (!strcasecmp (irr->cp, "!v"))       || 
      (!strcasecmp (irr->cp, "!-v"))      ||
      (!strcasecmp (irr->cp, "version"))) {
    irr->cp += 5;
    return_version(irr);
    return;
  }
  
  /* name of tool, !nroe  -- this should already be logged earlier */
  if (!strncasecmp (irr->cp, "!n", 2)) { 
    irr_send_okay (irr);
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
      if ((prefix = ascii2prefix (AF_INET, irr->cp)) == NULL)
	irr_send_error (irr, NULL);
      else {
	irr_exact (irr, prefix, SHOW_FULL_OBJECT);
	Delete_Prefix (prefix);
      }
    }
    else if (!strncmp (",l", cp, 2)) {
      *cp = '\0';
      if ((prefix = ascii2prefix (AF_INET, irr->cp)) == NULL) {
	irr_send_error (irr, NULL);
      }
      else {
	irr_less_all (irr, prefix, SEARCH_ONE_LEVEL, RAWHOISD_MODE);
	Delete_Prefix (prefix);
      }
    }
    else if (!strncmp (",L", cp, 2)) {
      *cp = '\0';
      if ((prefix = ascii2prefix (AF_INET, irr->cp)) == NULL) {
	irr_send_error (irr, NULL);
      }
      else {
	irr_less_all (irr, prefix, SEARCH_ALL_LEVELS, RAWHOISD_MODE);
	Delete_Prefix (prefix);
      }
    }
    else if (!strncasecmp (",o", cp, 2)) {
      *cp = '\0';
      if ((prefix = ascii2prefix (AF_INET, irr->cp)) == NULL) {
	irr_send_error (irr, NULL);
      }
      else {
	irr_exact (irr, prefix, SHOW_JUST_ORIGIN);
	Delete_Prefix (prefix);
      }
    }
    else if (!strncasecmp (",M", cp, 2)) {
      *cp = '\0';
      if ((prefix = ascii2prefix (AF_INET, irr->cp)) == NULL) {
	irr_send_error (irr, NULL);
      }
      else {
	irr_more_all (irr, prefix, RAWHOISD_MODE);
	Delete_Prefix (prefix);
      }
    }
    else {
      /* error */
      irr_send_answer (irr);
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
  
  /* j command.  Show the journal ranges.  See irr_journal_range for
     more info.  -* is valid, just like !s, but is implemented in
     the function. */
  if (!strncasecmp (irr->cp, "!j", 2)) {
    irr_journal_range (irr, irr->cp+2);
    return;
  }
  
  /* we are a mirror server, e.g. -g RADB:1:232-LAST */
  if (!strncasecmp (irr->cp, "-g", 2)) {
    irr_service_mirror_request (irr, irr->cp+2);
    return;
  }

  /* Process ripe whois commands.
   * ie, command does not start with "!". 
   */
  if (strncasecmp(irr->cp, "!", 1)) {
    irr_ripewhois(irr, irr->cp);
    return;      
  }
  
  /* error -- command unrecognized */
  sprintf (tmp, "F\n");
  irr_write_nobuffer (irr, irr->sockfd, tmp, strlen (tmp));
  
}

/* !m... rawhoisd command, eg "!man,as1234" */
void irr_m_command (irr_connection_t *irr) {
  int found = 0, i;
  char tmp[BUFSIZE];

  for (i = 0; i < IRR_MAX_MCMDS; i++) {
    if (!strncasecmp (irr->cp, m_info[i].command, strlen (m_info[i].command))) {
      found = 1;
      irr->cp += strlen (m_info[i].command);
      irr->ll_answer = LL_Create (LL_DestroyFunction, free, 0);

      irr_lock_all (irr);

      if (m_info[i].type == ROUTE)
        lookup_route_exact (irr, irr->cp);
      else
        irr_database_find_matches (irr, irr->cp, PRIMARY, 
                                   RAWHOISD_MODE|TYPE_MODE, m_info[i].type, 
                                   NULL, NULL);
      break;
    }
  }

  if (found) {
    send_dbobjs_answer (irr, DISK_INDEX, DB_OBJ, RAWHOISD_MODE);
    irr_unlock_all (irr);
    irr_write_buffer_flush (irr, irr->sockfd);
    LL_Destroy (irr->ll_answer);
  }
  else  {
    sprintf (tmp, "F\n");
    irr_write_nobuffer (irr, irr->sockfd, tmp, strlen (tmp));
  }

  return;
}

/* irr_ripewhois
 * Ripe whois commands, eg "whois AS2"
 */
void irr_ripewhois (irr_connection_t *irr, char * key) {
  prefix_t *prefix;

  /* New ripe flag stuff */
  /* want to retain fast output and recursive-off choices for
     ripe stay open mode while turning all other flags off */
  irr->ripe_flags &= (FAST_OUT | RECURS_OFF);
  if (parse_ripe_flags (irr, &key) < 0)
    return;

  
  if (irr->ripe_flags & TEMPLATE) {
    if (irr->ripe_type >= 0 && irr->ripe_type < IRR_MAX_KEYS)
      irr_write_nobuffer (irr, irr->sockfd, obj_template[irr->ripe_type], 
						 strlen (obj_template[irr->ripe_type]));
      return;
  }
  
  if (irr->ripe_flags & FAST_OUT)
    irr->short_fields = 1; 

  if ((irr->ripe_flags & SOURCES_ALL) &&
      (irr_set_ALL_sources (irr, RIPEWHOIS_MODE) == 0))
    return;

  if ((irr->ripe_flags & SET_SOURCE) && 
      (irr_set_sources (irr, irr->ripe_tmp, RIPEWHOIS_MODE) == 0))
    return;

  irr_lock_all (irr);
  irr->ll_answer = LL_Create (LL_DestroyFunction, free, 0);

  if (*key >= '0' && *key <= '9' &&
      (((irr->ripe_flags & OBJ_TYPE) == 0) ||
       irr->ripe_type == ROUTE)) {
    if (strchr (key, ':') != NULL)
      prefix = ascii2prefix (AF_INET6, key);
    else
      prefix = ascii2prefix (AF_INET, key);

    if (prefix) {
      /* New RIPE stuff */
      if (irr->ripe_flags & LESS_ALL)
        irr_less_all (irr, prefix, SEARCH_ALL_LEVELS, RIPEWHOIS_MODE);
      else if (irr->ripe_flags & (MORE_ONE | MORE_ALL))
        irr_more_all (irr, prefix, RIPEWHOIS_MODE);
      else
        irr_less_all (irr, prefix, SEARCH_ONE_LEVEL, RIPEWHOIS_MODE);
      Delete_Prefix (prefix);
    }
  }

  if (irr->ripe_flags & OBJ_TYPE) 
    irr_database_find_matches (irr, key, PRIMARY, TYPE_MODE, irr->ripe_type, 
                               NULL, NULL);
  else
    irr_database_find_matches (irr, key, PRIMARY, 0, 0, NULL, NULL);

  if ((irr->ripe_flags & (FAST_OUT | RECURS_OFF | OBJ_TYPE)) == 0)
    lookup_object_references (irr);  /* dupe ripe whois behavior */

  send_dbobjs_answer (irr, DISK_INDEX, DB_OBJ, RIPEWHOIS_MODE);
  irr_unlock_all (irr);
  irr_write_buffer_flush (irr, irr->sockfd);

  LL_Destroy (irr->ll_answer);
}


/* Get routes with specified community.  !hCOMM_NFSNET */
void irr_community (irr_connection_t *irr, char *name) {

  char comm_key[BUFSIZE];
  
  make_comm_key (comm_key, name);
  show_hash_spec_answer (irr, comm_key);
}

/* this is the !gas command, eg, !gas1234 gives list of
 * routes with origin as1234.
 */
void irr_origin (irr_connection_t *irr, char *name) {

  char gas_key[BUFSIZE];
  
  make_gas_key (gas_key, name);
  show_hash_spec_answer (irr, gas_key);
}

void show_hash_spec_answer (irr_connection_t *irr, char *key) {
  irr_database_t *db;
  int empty_answer = 1;
  hash_spec_t *hash_item;
  LINKED_LIST *ll;
  
  irr_lock_all (irr);
  irr->ll_answer = LL_Create (LL_DestroyFunction, free, 0);
  ll = LL_Create (LL_DestroyFunction, Delete_hash_spec, 0);

  LL_ContIterate (irr->ll_database, db) {

    if ((hash_item = fetch_hash_spec (db, key, FAST)) != NULL) {

      irr_build_answer (irr, db->fd, NO_FIELD, 0, 
			hash_item->len1, hash_item->gas_answer, RIPE181);
      /* JW: later put into a ll and destroy after showing answer 
       * Delete_hash_spec (hash_item);
       */
      LL_Add (ll, hash_item);

      empty_answer = 0;
    }
  }

  /* tack on a carriage return */
  if (!empty_answer)
    irr_build_answer (irr, NULL, NO_FIELD, 0, 1, "\n", RIPE181);
  send_dbobjs_answer (irr, MEM_INDEX, PRECOMP_OBJ, RAWHOISD_MODE);
  irr_unlock_all (irr);
  irr_write_buffer_flush (irr, irr->sockfd);
  LL_Destroy (irr->ll_answer);
  LL_Destroy (ll);
} /* new_irr_origin() */


/* Route searches.  L -  all level less specific eg, !r141.211.128/24,L
   Route searches.  l - one-level less specific eg, !r141.211.128/24,l
   Both these searches are inclusive (ie, the supplied route will be 
   returned if it exits.
   flag=0 -> "l" search; flag=1 -> "L" search */
void irr_less_all (irr_connection_t *irr, prefix_t *prefix, 
		   int flag, int mode) {
  radix_node_t *node = NULL;
  LINKED_LIST *ll_attr;
  irr_route_object_t *rt_object;
  irr_answer_t *irr_answer;
  irr_database_t *database;
  prefix_t *tmp_prefix = NULL;
  int route_found = 0;

  if (mode == RAWHOISD_MODE) {
     irr->ll_answer = LL_Create (LL_DestroyFunction, free, 0);
     irr_lock_all (irr);
  }
	
  LL_ContIterate (irr->ll_database, database) {
    route_found = 0;

    if (IRR.use_disk == 1) {
      if (tmp_prefix != NULL)
	Delete_Prefix (tmp_prefix);
      tmp_prefix = copy_prefix (prefix);
    }
    else
      tmp_prefix = prefix;

    /* first check if this node exists */
    if ((node = route_search_exact (database, database->radix, prefix)) != NULL) {
      ll_attr = (LINKED_LIST *) node->data;
      LL_Iterate (ll_attr, rt_object) {
	if (mode == RIPEWHOIS_MODE ||
	    irr->withdrawn == 1    || 
	    rt_object->withdrawn == 0) {
	  if (mode == RAWHOISD_MODE && irr->full_obj == 0)
	    irr_build_key_answer (irr, database->fd, database->name, ROUTE, 
				  rt_object->offset, rt_object->origin);
	  else
	    irr_build_answer (irr, database->fd, ROUTE, rt_object->offset, 
			      rt_object->len, NULL, database->db_syntax);
	}
      }
      if (IRR.use_disk == 1) IRRD_Delete_Node (node);
    }

    /* now check all less specific */
    while ((flag == SEARCH_ALL_LEVELS || node == NULL) && 
	   (node = route_search_best (database, database->radix, 
				      tmp_prefix)) != NULL) {
      if (node != NULL) { 
	if (IRR.use_disk == 1) {
	  if (tmp_prefix != NULL)
	    Delete_Prefix (tmp_prefix);
	  tmp_prefix = copy_prefix (node->prefix);
	}
	else
	  tmp_prefix = node->prefix;

	ll_attr = (LINKED_LIST *) node->data;
	LL_Iterate (ll_attr, rt_object) {
	  if (mode == RIPEWHOIS_MODE ||
	      irr->withdrawn == 1    || 
	      rt_object->withdrawn == 0) {
	    route_found = 1;
	    if (mode == RAWHOISD_MODE && irr->full_obj == 0)
	      irr_build_key_answer (irr, database->fd, database->name, ROUTE, 
				    rt_object->offset, rt_object->origin);
	    else {
	      irr_build_answer (irr, database->fd, ROUTE, rt_object->offset, 
				rt_object->len, NULL, database->db_syntax);
            }
	  }
	}

	if (IRR.use_disk == 1) {
	  IRRD_Delete_Node (node);
	  node = NULL;
	}
      }
      /* break out after one loop for "l" case */
      if (route_found && (flag == SEARCH_ONE_LEVEL)) break; 
    }
  }
  
  /* a little cleanup */
  if ((IRR.use_disk == 1) && (tmp_prefix != NULL))
    Delete_Prefix (tmp_prefix);
  if ((IRR.use_disk == 1) && (node != NULL))
    IRRD_Delete_Node (node);

  if (mode == RAWHOISD_MODE) {
    if (irr->full_obj == 0) {
      send_dbobjs_answer (irr, MEM_INDEX, PRECOMP_OBJ, RAWHOISD_MODE);
      LL_ContIterate (irr->ll_answer, irr_answer) {
	free (irr_answer->blob);
      }
    }
    else
      send_dbobjs_answer (irr, DISK_INDEX, DB_OBJ, RAWHOISD_MODE);
    irr_unlock_all (irr);
    irr_write_buffer_flush (irr, irr->sockfd);
    LL_Destroy(irr->ll_answer);
  }
}

/* Route searches. M - all more specific eg, !r199.208.0.0/16,M */
/* Also does m - one level only more specific */
void irr_more_all (irr_connection_t *irr, prefix_t *prefix, int mode) {
  radix_node_t *node, *start_node, *last_node;
  LINKED_LIST *ll_attr;
  irr_route_object_t *rt_object;
  irr_answer_t *irr_answer;
  irr_database_t *database;

  
  if (IRR.use_disk == 1) {
    irr_send_error (irr, "This command only support in memory-only mode");
    return;
  }

  if (prefix->bitlen < 20) {
    irr_send_error (irr, "We only allow more searches > /20");
    return;
  }

  if (mode == RAWHOISD_MODE) {
    irr->ll_answer = LL_Create (LL_DestroyFunction, free, 0);
    irr_lock_all (irr);
  }

  LL_ContIterate (irr->ll_database, database) {

    last_node = NULL;
    start_node = NULL;
    node = NULL;    

    /* do the search on disk */
    if (IRR.use_disk == 1) {
      /* this doesn't really work yet... */
      /*route_search_more_specific (irr, database, prefix);*/
      continue;
    }
    /* ----  end disk search  ----- */

    /* memory  -- find the prefix, or the best large node */
    start_node = radix_search_exact_raw (database->radix, prefix);

    if (start_node != NULL) {
      RADIX_WALK (start_node, node) {
	trace (TRACE, default_trace, "  -M %s\n", prefix_toax (node->prefix));
	if ((node->prefix != NULL) &&
	    (node->prefix->bitlen >= prefix->bitlen) &&
	    (comp_with_mask ((void *) prefix_tochar (node->prefix), 
			     (void *) prefix_tochar (prefix),  prefix->bitlen))) {
	  ll_attr = (LINKED_LIST *) node->data;
	  LL_Iterate (ll_attr, rt_object) {
	    if ((irr->withdrawn == 1) || (rt_object->withdrawn == 0)) {
	      if (irr->full_obj == 0)
		irr_build_key_answer (irr, database->fd, database->name, ROUTE, 
				      rt_object->offset, rt_object->origin);
	      else
		irr_build_answer (irr, database->fd, ROUTE, rt_object->offset, 
				  rt_object->len, NULL, database->db_syntax);
	    }
	  }
	}
      }
      RADIX_WALK_END;
    }
  }
  
  if (mode == RAWHOISD_MODE) {
    if (irr->full_obj == 0) {
      send_dbobjs_answer (irr, MEM_INDEX, PRECOMP_OBJ, RAWHOISD_MODE);
      LL_ContIterate (irr->ll_answer, irr_answer) {
        free (irr_answer->blob);
      }
    }
    else
      send_dbobjs_answer (irr, DISK_INDEX, DB_OBJ, RAWHOISD_MODE);
    irr_unlock_all (irr);
    irr_write_buffer_flush (irr, irr->sockfd);
    LL_Destroy(irr->ll_answer);
  }

}

/* Route searches.  o - return origin of exact match(es) eg, !r141.211.128/24,o 
 * flag == 1 means just return the ASorigin. If not 1, return the full object 
 */
void irr_exact (irr_connection_t *irr, prefix_t *prefix, int flag) {
  radix_node_t *node = NULL;
  LINKED_LIST *ll_attr;
  irr_route_object_t *rt_object;
  irr_database_t *database;
  irr_answer_t *irr_answer;
  char buffer[BUFSIZE], *p;
  int first = 1;

  if (flag == SHOW_FULL_OBJECT)
    irr->ll_answer = LL_Create (LL_DestroyFunction, free, 0);
  else
    buffer[0] = '\0';

  irr_lock_all (irr);
  LL_ContIterate (irr->ll_database, database) {

    node = route_search_exact (database, database->radix, prefix);

    if (node != NULL) { 
      ll_attr = (LINKED_LIST *) node->data;
      LL_Iterate (ll_attr, rt_object) {
	if (irr->withdrawn == 1 || rt_object->withdrawn == 0) {
	  if (flag == SHOW_JUST_ORIGIN) {
	    if (irr->full_obj == 0) {
	      if (first != 1) irr_add_answer (irr, "\n");
	      first = 0;
	      irr_add_answer (irr, "%s AS%d", database->name, rt_object->origin);
	    }
	    else {
	      /* irr_add_answer (irr, "AS%d", rt_object->origin); */
	      p = buffer + strlen (buffer);
	      sprintf (p, "AS%d ", rt_object->origin);
	    }
	  }
	  else {
	    if (irr->full_obj == 1)
	      irr_build_answer (irr, database->fd, ROUTE, rt_object->offset, 
				rt_object->len, NULL, database->db_syntax);
	    else 
	      irr_build_key_answer (irr, database->fd, database->name, ROUTE, 
				    rt_object->offset, rt_object->origin);
	  }
	}
      }
      if (IRR.use_disk == 1) {
	IRRD_Delete_Node (node);
	node = NULL;
      }
    }
  }
  
  /* cleanup */
  if ((IRR.use_disk == 1) && (node != NULL)) 
    IRRD_Delete_Node (node);
  
  if (flag == SHOW_JUST_ORIGIN) {
    if (irr->full_obj == 1 && strlen (buffer) > 0)
      irr_add_answer (irr, "%s", buffer); 
    irr_send_answer (irr);
  }
  else {
    if (irr->full_obj == 1)
      send_dbobjs_answer (irr, DISK_INDEX, DB_OBJ, RAWHOISD_MODE);
    else {
      send_dbobjs_answer (irr, MEM_INDEX, PRECOMP_OBJ, RAWHOISD_MODE);
      LL_ContIterate (irr->ll_answer, irr_answer) {
	free (irr_answer->blob);
      }
    }
  }   

  irr_unlock_all (irr);
  irr_write_buffer_flush (irr, irr->sockfd);
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
      irr_send_error (irr, NULL);
    else
      irr_send_okay (irr);
  }
  else if (buf[0] != '\0') /* in RIPEWHOIS_MODE and errors, tell user */
    irr_write_nobuffer (irr, irr->sockfd, buf, strlen (buf));

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
      irr_send_error (irr, NULL);
    else
      irr_send_okay (irr);
  }
  else if (buf[0] != '\0')  /* in RIPEWHOIS_MODE and errors, tell user */
      irr_write_nobuffer (irr, irr->sockfd, buf, strlen (buf));

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
	char db_canon[BUFSIZE];

	db_canon[BUFSIZE - 1] = '\0';
	strncpy (db_canon, db, BUFSIZE-1);
	convert_toupper(db_canon);

	trace (NORM, default_trace, "ERROR -- Database not found %s\n", db_canon);
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
  char db_canon[BUFSIZE], *p_last_export;
  u_long oldest_serial, current_serial, last_export = 0L;

  db_canon[BUFSIZE - 1] = '\0';
  strncpy (db_canon, irr->database->name, BUFSIZE-1);
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

