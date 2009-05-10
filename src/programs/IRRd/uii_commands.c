/*
 * $id: uii_commands.c,v 1.1.1.1 1998/08/10 20:06:56 dogcow Exp $
 * originally Id: uii_commands.c,v 1.19 1998/06/15 16:20:36 gerald Exp 
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
#include "irrd.h"

#include <fcntl.h>
#ifndef SETPGRP_VOID
#endif

#ifndef localtime_r /* Not present on SunOS, at least. */
#define localtime_r(a,b) localtime(a)
#endif

extern trace_t *default_trace;

/* Handler routines for IRR lookups
 * the show_xxx functions are human readable UII commands
 * the irr_xxx are used by the RAWhoisd machine telnet interface
 */
void uii_show_ip_less (uii_connection_t *uii, prefix_t *prefix, int flag);
void uii_show_ip_exact (uii_connection_t *uii, prefix_t *prefix);
void uii_show_ip_more (uii_connection_t *uii, prefix_t *prefix);
static void run_cmd (char *cmd, FILE **in, FILE **out);

void show_database (uii_connection_t *uii) {
  irr_database_t *database;
  char buf[BUFSIZE], tmp[BUFSIZE], *p_last_export;
  int total_size, total_rt, total_aut;

  uii_add_bulk_output (uii, "Listening on port %d (fd=%d)", IRR.irr_port, IRR.sockfd);
  if (IRR.whois_port > 0) {
    uii_add_bulk_output (uii, ", and port %d (fd=%d)", IRR.whois_port, IRR.sockfd);
  }
  uii_add_bulk_output (uii, "\r\n");

  uii_add_bulk_output (uii, "Memory-only indexing\r\n");

  uii_add_bulk_output (uii, "Default Database Query Order: ");
  LL_Iterate (IRR.ll_database, database) {
    uii_add_bulk_output (uii, "%s ", database->name);
  }

  uii_add_bulk_output (uii, "\r\n\r\n");

  uii_add_bulk_output (uii, "%10s       %-9s    %6s    %10s    %8s    %s", 
		       "Database", "Size (kb)", "Rt Obj",
		       "AutNum Obj", "Serial #", "Last Export #\r\n");
  uii_add_bulk_output (uii, "%6s    %-9s    %6s    %10s    %4s    %s", 
		       "-------------", "---------", "------",
		       "----------", "--------", "-------------\r\n");

  total_size = total_rt = total_aut = 0;

  LL_Iterate (IRR.ll_database_alphabetized, database) {
	sprintf (buf, "%8.1f", ((float) database->bytes / 1000.0));
    uii_add_bulk_output (uii, " %-12s    %9s", 
			 database->name, buf);

    strcpy (tmp, database->name);
    convert_toupper (tmp);
    p_last_export = GetStatusString (IRR.statusfile, tmp, "lastexport");

    uii_add_bulk_output (uii, "    %6d    %10d    %8u    %13s\r\n", 
			 database->num_objects[ROUTE],
			 database->num_objects[AUT_NUM],
			 database->serial_number,
			 (p_last_export == NULL) ? "" : p_last_export);

    total_size += database->bytes;
    total_rt += database->num_objects[ROUTE];
    total_aut += database->num_objects[AUT_NUM];
  }

  sprintf (buf, "%8.1f", ((float) total_size / 1000.0));
  uii_add_bulk_output (uii, " %-12s    %9s    %6d    %10d\r\n",
                       "TOTAL", buf, total_rt, total_aut);

  uii_add_bulk_output (uii, "\r\n");

  LL_Iterate (IRR.ll_database_alphabetized, database) {
    uii_add_bulk_output (uii, "%s ", database->name);
    if (database->flags & IRR_AUTHORITATIVE)
      uii_add_bulk_output (uii, " AUTHORITATIVE");
    if (database->obj_filter > 0) 
      uii_add_bulk_output (uii, " (filter 0x%x)", database->obj_filter);
    uii_add_bulk_output (uii, "\r\n");

    /* mirroring */
    if (database->mirror_prefix != NULL) {
      nice_time (time_left (database->mirror_timer), buf);
      uii_add_bulk_output (uii, "   Mirroring %s:%d (Next in %s)\r\n", 
			   prefix_toa (database->mirror_prefix),
			   database->mirror_port, 
			   buf);
      if (database->mirror_error_message[0] != '\0') {
	if (database->mirror_error_message[strlen(database->mirror_error_message) -1] == '\n')
	  database->mirror_error_message[strlen(database->mirror_error_message) -1] = '\0';
	uii_add_bulk_output (uii, "   %s \r\n", database->mirror_error_message);
      }
      if (database->mirror_fd != -1) {
	uii_add_bulk_output (uii, "   MIRRORING (%d bytes) (%d seconds)\r\n",
			     database->mirror_update_size,
			     time (NULL) - database->mirror_started);
      }
      else if (database->last_mirrored <= 0) {
	uii_add_bulk_output (uii, "   Never mirrored\r\n");
      }
      else {
	char tmpt[BUFSIZE];
	strftime (tmpt, BUFSIZE, "%T %m/%d/%Y", 
		  localtime_r ((time_t *) &database->last_mirrored, &my_tm));
	uii_add_bulk_output (uii, "   Last mirrored %s\r\n", tmpt);
	uii_add_bulk_output (uii, "   %d bytes, %d change(s)\r\n", 
			     database->mirror_update_size, database->num_changes);
      }
    }
    /*
     * mirroring not turned on 
     * Either loaded, or email
     */
    else {
      char tmpt[BUFSIZE];

      if (database->flags & IRR_AUTHORITATIVE) {
	if (database->last_update == 0)
	  uii_add_bulk_output (uii, "   Last email/tcp update Never\r\n");
	else {
	  strftime (tmpt, BUFSIZE, "%T %m/%d/%Y", 
		    localtime_r ((time_t *) &database->last_update, &my_tm));
	  uii_add_bulk_output (uii, "   Last email/tcp update %s\r\n", tmpt);
	}
      }

      if (database->time_loaded == 0)
	uii_add_bulk_output (uii, "   Last loaded Never\r\n");
      else {
	strftime (tmpt, BUFSIZE, "%T %m/%d/%Y", 
		  localtime_r ((time_t *) &database->time_loaded, &my_tm));
	uii_add_bulk_output (uii, "   Last loaded %s\r\n", tmpt);
      }
    } 

    if (database->export_timer != NULL) {
      nice_time (time_left (database->export_timer), buf);
      uii_add_bulk_output (uii, "   Next export in %s\r\n", buf);
    }


    if (database->no_dbclean) {
      uii_add_bulk_output (uii, "   Cleaning disabled\r\n");
    }
    else if (database->clean_timer != NULL) {
      nice_time (time_left (database->clean_timer), buf);
      uii_add_bulk_output (uii, "   Next dbclean in %s\r\n", buf);
    }

  }
  uii_send_bulk_data (uii);
}


/* uii_irr_reload 
 * Reload the database 
 */
void uii_irr_reload (uii_connection_t *uii, char *name) {
  if (name != NULL) {
    irr_reload_database (name, uii, NULL);
    Delete (name);
  }
}

/* Invoke irrdcacher to fetch a DB and call 'irr_reload_database ()'
 * to reload it.
 *
 * sample uii command: 'irrdcacher bell'
 * 
 * Function can be called from the uii or from 'irr_load_data ()'
 * which is invoked in main.c on bootstrap.  'irr_load_data ()'
 * loads the DB atomically
 *
 * Function relies on irrdcacher and wget being installed and in
 * the search path.  The search path can be augmented with the
 * 'irr_path' config command.  There are no assumptions about
 * the availability of irrdcacher and if found this routine will
 * parse the irrdcacher output to determine the result.
 *
 * Input:
 *  -data struct for communicating with UII users (uii)
 *  -DB name to fetch (name)
 *
 * Return:
 *  -1 if the DB wsa fetched and reloaded without error
 *  -0 otherwise
 */
int uii_irr_irrdcacher (uii_connection_t *uii, char *name) {
  int ret_code = 0;
  FILE *pipeout;
  char cmd[BUFSIZE], cs[BUFSIZE], wflag[BUFSIZE], *tmp_dir;
  irr_database_t *db;
  regex_t url_re = {0}, good_re = {0}, notfound_re = {0};
  char *ftp_url  = "^ftp://[^ \t/]+/[[:graph:]]+$";
  char *good     = "^Successful operation";
  char *notfound = "not found[ \t]*\n$";

  /* sanity checks */

  /* do we know about this DB? */
  if (name == NULL ||
      (db = find_database (name)) == NULL) {
    if (uii != NULL)
      uii_send_data (uii, "Unknown DB (%s)\r\n", ((name == NULL) ? "NULL" : name));
    goto ABORT_IRRDCACHER;
  }
  /* it's a mistake to fetch an authoritative DB */
  else if (db->flags & IRR_AUTHORITATIVE) {
    if (uii != NULL)
      uii_send_data (uii, "ERROR: (%s) is declared \"authoritative\" \r\n", name);
    goto ABORT_IRRDCACHER;
  }
  /* see if a remote ftp dir has been configured */
  else if (db->remote_ftp_url == NULL) {
    if (uii != NULL) {
      uii_send_data (uii, 
		     "No ftp remote directory has been configured for (%s)\r\n", name);
      uii_send_data (uii, "Type \"config\" and then \"irr_database %s remote_ftp_url <ftp URL>\"\r\n", name);
    }
    goto ABORT_IRRDCACHER;
  } 

  /* compile our reg ex's */
  regcomp (&url_re,      ftp_url,     REG_EXTENDED|REG_NOSUB);
  regcomp (&good_re,     good,        REG_EXTENDED|REG_NOSUB);
  regcomp (&notfound_re, notfound,    REG_EXTENDED|REG_NOSUB);

  /* do we have a well formed ftp URL? */
  if (regexec (&url_re, db->remote_ftp_url, 0, NULL, 0)) {
    if (uii != NULL) {
      uii_send_data (uii, "Invalid ftp URL for (%s)!\r\n", db->name);
      uii_send_data (uii, "Expect (ftp://<remote server>/<remote directory>)\r\n", name);
    }
    goto ABORT_IRRDCACHER;
  }

  /* build the irrdcacher command invokation */

  /* IRR.path is used to add a component to the current path, ie,
   * might be needed to find irrdcacher and wget */
  cmd[0] = wflag[0] = '\0';
  if (IRR.path != NULL) {
    sprintf (cmd, "PATH=${PATH}%s%s;export PATH;", ((*IRR.path == ':') ? "" : ":"),
	     IRR.path);

    sprintf (wflag, "-w %s", IRR.path);
  }

#ifdef HAVE_WGET
  {  char *p;
  
  if (wflag[0] == '\0')
    sprintf (wflag, "-w ");

  sprintf (&wflag[strlen (wflag)], ":%s", WGET_CMD);
  if ((p = strrchr (wflag, '/')) != NULL)
    *p = '\0';
  }
#endif
  
  trace (NORM, default_trace, "JW: wflag (%s)\n", wflag);

  /* dir for irrdcacher to use to place to bring the files onto our host */
  if ((tmp_dir = IRR.tmp_dir) == NULL)
    tmp_dir = "/var/tmp";
  
  /* build the irrdcacher invokation */
#ifdef HAVE_IRRDCACHER
  sprintf (&cmd[strlen (cmd)], "%s", IRRDCACHER_CMD);
#else
  sprintf (&cmd[strlen (cmd)], "%s", "irrdcacher");
#endif
  sprintf (&cmd[strlen (cmd)], 
	   " -S %s -s %s %s.db.gz ", wflag,  
	   db->remote_ftp_url, name);

  /* we need the *.CURRENTSERIAL file also for mirrored files */
  if (db->mirror_prefix != NULL) {
    /* need to make the DB name upper case */
    strcpy (cs, name);
    convert_toupper (cs);
    sprintf (&cmd[strlen (cmd)], " %s.CURRENTSERIAL", cs);
  }

  trace (NORM, default_trace, "JW: cmd (%s)\n", cmd);

  if (uii != NULL)
    uii_send_data (uii, "Fetching DB remotely.  Please be patient...\r\n");

  /* invoke irrdcacher */
  run_cmd (cmd, NULL, &pipeout);
  if (pipeout == NULL) {
    if (uii != NULL)
      uii_send_data (uii, "irrdcacher not found or no \"sh\" found.\r\n");
    goto ABORT_IRRDCACHER;
  }

  /* parse the irrdcacher output, look at the last line only */
  cmd[0] = '\0';
  while (fgets (cmd, BUFSIZE, pipeout) != NULL) {
    trace (NORM, default_trace, "irrdcacher: %s", cmd);
    if (uii != NULL)
      uii_send_data (uii, "%s", cmd);
  }
  fclose (pipeout);

  /* move the DB into our cache and reload */  
  if (regexec (&good_re, cmd, 0, NULL, 0)  == 0)
    ret_code = irr_reload_database (name, uii, tmp_dir);

  /* "sh" not installed */
  else if (cmd[0] == '\0') {
    if (uii != NULL)
      uii_send_data (uii, "/bin/sh not installed.  Could not run irrdacher.\r\n");
  }
  /* irrdcacher not found */
  else if (regexec (&notfound_re, cmd, 0, NULL, 0)  == 0) {
    if (uii != NULL) {
      uii_send_data (uii, "Couldn't find irrdcacher on your system\r\n");
      uii_send_data (uii, "irrd requires both 'irrdcacher' and 'wget' for remote ftp access\r\n");
      uii_send_data (uii, "Reset your path with \"irr_path\".  eg, irr_path /usr/mybin\r\n");
    }
  }
  else if (uii != NULL) {
    uii_send_data (uii, "%s\n", cmd);
    uii_send_data (uii, "Operation unsuccessful!\n");
  }

ABORT_IRRDCACHER:

  /* clean up */
  Delete  (name);
  regfree (&good_re);
  regfree (&url_re);
  regfree (&notfound_re);

  return ret_code;
}

void uii_export_database (uii_connection_t *uii, char *name) {
  irr_database_t *db;

  db = find_database (name);

  if (IRR.ftp_dir == NULL) {
    uii_send_data (uii, "Error -- no ftp directory configured.\r\n");
    Delete (name);
    return;
  }

  if (db != NULL) {   
    uii_send_data (uii, "Exporting %s database to %s...\r\n", 
		   name, IRR.ftp_dir);
    if (irr_database_export (db) != 1) {
       uii_send_data (uii, "ERROR exporting %s database -- see log for more information\r\n", 
		      name);
    }
    else {
      uii_send_data (uii, "Export successful for %s\r\n", name);
      
      if (db->export_timer != NULL) 
	Timer_Reset_Time (db->export_timer);
    }
  }
  else 
    uii_send_data (uii, "Could not find %s database...\r\n", name);

  Delete (name);
  return;
}

void uii_irr_clean (uii_connection_t *uii, char *name) {
  irr_database_t *db;

  db = find_database (name);

  if (db != NULL) {   
    uii_send_data (uii, "Cleaning %s database...\r\n", name);
    irr_database_clean (db);
    if (db->clean_timer != NULL)
      Timer_Reset_Time (db->clean_timer);
  }
  else 
    uii_send_data (uii, "Could not find %s database...\r\n", name);

  Delete (name);
  return;
}

void uii_irr_mirror_last (uii_connection_t *uii, char *name) {
  uii_irr_mirror (uii, name, 0);
}

void uii_irr_mirror_serial (uii_connection_t *uii, char *name, uint32_t serial) {
  uii_irr_mirror (uii, name, serial);
}

void uii_irr_mirror (uii_connection_t *uii, char *name, uint32_t serial) {
  irr_database_t *database;

  database = find_database (name);
  
  if (database == NULL) {
    config_notice (ERROR, uii, "Database not found\r\n");
    Delete (name);
    return;
  }

  if (database->mirror_prefix == NULL) {
    uii_send_data (uii, "Don't know how to mirror %s\r\n", database->name);
    return;
  }
  
  uii_send_data (uii, "Mirroring %s database...\r\n", name);
  uii_send_data (uii, "Current serial number %u\r\n", database->serial_number);
  if (serial > 0)
    uii_send_data (uii, "serial to: %u\n", serial);
  else
    uii_send_data (uii, "serial to: LAST\n");
  uii_send_data (uii, "Starting mirror in the background......\r\n", 
		 database->serial_number);

  if (request_mirror (database, uii, serial) == -1) {
    uii_send_data (uii, "\r\n** ERROR ** mirroring database!\r\n");
    uii_send_data (uii, "See logfile for more information\r\n");
    Delete (name);
    return;
  }

  if (database->mirror_timer != NULL) 
    Timer_Reset_Time (database->mirror_timer);

  Delete (name);
  return;
}

/* mainly for debugging, but would be nice to be able to set the serial number */
void uii_set_serial (uii_connection_t *uii, char *name, uint32_t serial) {
  irr_database_t *database;

  if ((database = find_database (name)) != NULL) {
    database->serial_number = serial;
    Delete (name);
    return;
  }
  
  uii_send_data (uii, "%s database not found\r\n", name);
  Delete (name);
}

void uii_show_ip (uii_connection_t *uii, prefix_t *prefix, int num, char *lessmore) {
  if (num == 0) {
    uii_show_ip_exact (uii, prefix);
    return;
  }

  if (!strcmp (lessmore, "less")) {
    uii_show_ip_less (uii, prefix, 1);
    return;
  }
  else if (!strcmp (lessmore, "more")) {
    uii_show_ip_more (uii, prefix);
    return;
  } else {
    uii_add_bulk_output (uii, "Unrecognized option -- use \"less\" or \"more\"\r\n");
    uii_send_bulk_data (uii);
  }
  return;
}

void uii_fetch_irr_object (uii_connection_t *uii, 
			   irr_database_t *database, u_long offset) {
  char buffer[BUFSIZE];
  size_t len;
  int continue_line = 0;
  
  fseek (database->db_fp, offset, SEEK_SET);
    
  while (fgets (buffer, BUFSIZE, database->db_fp) != NULL) {
    len = strlen(buffer);
    if (len == 1 && !continue_line) break; /* single newline is end of obj */
    if (buffer[len - 1] != '\n') { /* line length exceeds buffer */
      continue_line = 1;
      uii_add_bulk_output (uii, "%s", buffer);
    } else {
      continue_line = 0;
      buffer[len - 1] = '\0'; /* lose newline */
      uii_add_bulk_output (uii, "%s\r\n", buffer);
    }
  }
}

void uii_show_ip_exact (uii_connection_t *uii, prefix_t *prefix) {
  radix_node_t *node;
  irr_prefix_object_t *prefix_object;
  irr_database_t *database;
  int first = 1;

  LL_Iterate (IRR.ll_database, database) {
    irr_lock (database);

    node = prefix_search_exact (database, prefix);

    if (node != NULL) { 
      prefix_object = (irr_prefix_object_t *) node->data;
      while (prefix_object != NULL) {
	if (first != 1) uii_add_bulk_output (uii, "\r\n");
	first = 0;
	uii_fetch_irr_object (uii, database, prefix_object->offset);
	prefix_object = prefix_object->next;
      }
    }
    irr_unlock (database);
  }    
  uii_send_bulk_data (uii);
}

void uii_show_ip_less (uii_connection_t *uii, prefix_t *prefix, int flag) {
  radix_node_t *node;
  irr_prefix_object_t *prefix_object;
  irr_database_t *database;
  prefix_t *tmp_prefix;
  int first = 1;

  LL_Iterate (IRR.ll_database, database) {
    irr_lock (database);
    
    tmp_prefix = prefix;

    /* first check if this node exists */
    if ((node = prefix_search_exact (database, prefix)) != NULL) {
      prefix_object = (irr_prefix_object_t *) node->data;
      while (prefix_object != NULL) {
	if (first != 1) uii_add_bulk_output (uii, "\r\n");
	first = 0;
	uii_fetch_irr_object (uii, database, prefix_object->offset);
	prefix_object = prefix_object->next;
      }
    }
    /* now check all less specific */
    while ((flag == 1 || node == NULL) && 
	   (node = prefix_search_best (database, tmp_prefix)) != NULL) {
      if (node != NULL) { 
	tmp_prefix = node->prefix;
	prefix_object = (irr_prefix_object_t *) node->data;
	while (prefix_object != NULL) {
	  if (first != 1) uii_add_bulk_output (uii, "\r\n");
	  first = 0;
	  uii_fetch_irr_object (uii, database, prefix_object->offset);
	  prefix_object = prefix_object->next;
	}
      }
      /* break out after one loop for "l" case */
      if (flag == 0) break; 
    }
    irr_unlock (database);
  }
  uii_send_bulk_data (uii);
}

/* More specific route search */ 
void uii_show_ip_more (uii_connection_t *uii, prefix_t *prefix) {
  radix_node_t *node, *start_node, *last_node;
  radix_tree_t *radix;
  irr_prefix_object_t *prefix_object;
  irr_database_t *database;
  int first = 1;

  LL_Iterate (IRR.ll_database, database) {
    irr_lock (database);

    last_node = NULL;
    start_node = NULL;
    node = NULL;    

    if (prefix->family == AF_INET6)
      radix = database->radix_v6;
    else
      radix = database->radix_v4;

    /* find this prefix exactly or the best large node */
    start_node = radix_search_exact_raw (radix, prefix);

    if (start_node != NULL) {
      RADIX_WALK (start_node, node) {
	if ((node->prefix != NULL) &&
	    (node->prefix->bitlen >= prefix->bitlen) &&
	    (comp_with_mask ((void *) prefix_tochar (node->prefix), 
			     (void *) prefix_tochar (prefix),  prefix->bitlen))) {
	  prefix_object = (irr_prefix_object_t *) node->data;
	  while (prefix_object != NULL) {
	    if (first != 1) {
	      uii_add_bulk_output (uii, "\r\n");
	    }
	    first = 0;
	    uii_fetch_irr_object (uii, database, prefix_object->offset);
	    prefix_object = prefix_object->next;
	  }
	}
      }
      RADIX_WALK_END;
    }
    irr_unlock (database);
  }
  uii_send_bulk_data (uii);
}

/* uii_delete_route
 * mainly for testing/debugging 
 */
int uii_delete_route (uii_connection_t *uii, char *name, prefix_t *prefix, int as) {
  irr_database_t *db;
  irr_object_t *irr_object;
  int ret;

  db = find_database (name);

  if (db == NULL) {   
    uii_send_data (uii, "Could not find %s database...\r\n", name);
    Deref_Prefix(prefix);
    Delete (name);
    return (-1);
  }
  Delete (name);    

  irr_object = New (irr_object_t);
  irr_object->type = ROUTE;
  irr_object->name = prefix_toax (prefix);
  irr_object->origin = as;

  irr_update_lock (db);
  if ((irr_object = load_irr_object (db, irr_object)) == NULL) {
    irr_update_unlock (db);
    uii_send_data (uii, "Could not find find %s...\r\n", prefix_toax (prefix));
    Deref_Prefix(prefix);
    return (-1);
  }

  ret = delete_irr_prefix (db, prefix, irr_object);
  irr_update_unlock (db);
  if (ret) 
    uii_send_data (uii, "Deleted\r\n");
  else
    uii_send_data (uii, "Deleted failed!\r\n");

  return (1);
}

int uii_read_update_file (uii_connection_t *uii, char *file, char *name) {
  irr_database_t *db;
  uint32_t new;
  FILE *fp;
  
  db = find_database (name);

  if (db == NULL) {   
    uii_send_data (uii, "Could not find %s database...\r\n", name);
    Delete (name);
    return (-1);
  }
  Delete (name); 

  if ((fp = fopen (file, "r")) == NULL) {
    uii_send_data (uii, "Could not open %s\r\n", file);
    return (-1);
  }

  /* just kill of the start line */
  valid_start_line (db, fp, &new);

  irr_update_lock (db);
  if (scan_irr_file (db, "load", 2, fp) != NULL) {
    trace (NORM, default_trace, 
	   "Read of updates failed: serial number unchanged: %u\n",
	   db->serial_number);
  }
  irr_update_unlock (db);
  fclose (fp);

  return (1);
}

void run_cmd (char *cmd, FILE **in, FILE **out) {
  int pin[2], pout[2];

  if (in != NULL)
    *in = NULL;

  if (out != NULL)
    *out = NULL;
  
  if (in != NULL)
    pipe (pin);

  if (out != NULL)
    pipe (pout);
  
  if (fork() == 0) { /* We're the child */
    if (in != NULL) {
      close (pin[1]);
      dup2  (pin[0], 0);
      close (pin[0]);
    }
    
    if (out != NULL) {
      close (pout[0]);
      dup2  (pout[1], 1);
      dup2  (pout[1], 2);
      close (pout[1]);
    }
    
    execl("/bin/sh", "sh", "-c", cmd, NULL);
    _exit(127);
  }

  /* Only get here if we're the parent */
  if (out != NULL) {
    close (pout[1]);
    *out = fdopen (pout[0], "r");
  }
  
  if (in != NULL) {
    close (pin[0]);
    *in = fdopen (pin[1], "w");
  }
}

int kill_irrd (uii_connection_t *uii) {

  uii_send_data (uii, "Are you sure (yes/no)? ");
  if (uii_yes_no (uii)) {
    uii_send_data (uii, "Exit requested\n");
    exit(0);

    /* XXX this code does not work -ljb */
    /* need to mutex_init(irr->lock_all_mutex_lock) ? */
#ifdef notdef
    irr = New (irr_connection_t);
    irr->ll_database = IRR.ll_database;
    irr_lock_all (irr);
    mrt_set_force_exit (MRT_FORCE_EXIT);
#endif
  }
  return (0);
}
