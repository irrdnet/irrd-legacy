/*
 * $Id: mirror.c,v 1.8 2002/02/04 20:53:56 ljb Exp $
 * originally Id: mirror.c,v 1.26 1998/08/03 21:55:23 gerald Exp 
 */

#include <stdio.h>
#include <string.h>
#include "mrt.h"
#include "trace.h"
#include <time.h>
#include <signal.h>
#include "config_file.h"
#include "select.h"
#include <ctype.h>
#include "irrd.h"
#include "irrd_prototypes.h"

extern trace_t *default_trace;

static int mirror_read_data ();
int dump_serial_updates (irr_connection_t *irr, char * dbname, int journal_ext, 
			 u_long from, u_long to);
int build_update (char *file, irr_connection_t *irr, FILE *fp, u_long curr_serial, 
			 u_long to);
int valid_start_line (irr_database_t *db, FILE *fp, u_long *serial_num);

int request_mirror (irr_database_t *db, uii_connection_t *uii, int last) {
  char tmp[BUFSIZE], name[BUFSIZE], logfile[BUFSIZE];
  struct timeval	tv;
  fd_set		write_fds;
  int			n, i, ret;

  if (db->db_fp == NULL) {
    trace (NORM, default_trace, 
	   "Aborting mirror -- we don't have an open file pointer for %s\n",
	   db->name);
    return (-1);
  }

  if (last > 0 &&
     (db->serial_number + 1) > last) {
    trace (NORM, default_trace,
	   "ERROR: user supplied serial # last (%d) is <= current serial (%d)\n", 
	   last, db->serial_number);
    return (-1);
  }

  db->mirror_error_message[0] = '\0';

  /* 
   * Check if already mirroring. 
   * If so, check if we want to time out old attempt 
   */
  if (db->mirror_fd != -1) {
    if ( (time (NULL) - db->mirror_started) < MIRROR_TIMEOUT) {
      trace (NORM, default_trace, "**** ERROR **** %s already mirroring...!\n",
	     db->name);
      /*irr_unlock (db);*/
      return (-1);
    }

    /*irr_unlock (db);*/
    trace (NORM, default_trace, "**** TIMING OUT old mirror attempt to %s\n",
	   db->name);
    strcpy (db->mirror_error_message, "Mirroring timed out...");
    select_delete_fd (db->mirror_fd);
    fclose (db->mirror_disk_fp);
    db->mirror_disk_fp = NULL;
    db->mirror_fd = -1;
    return (-1);
  }

  /*irr_unlock (db);*/

  if ((db->mirror_fd = socket (AF_INET, SOCK_STREAM, 0)) < 0) {
    trace (NORM, default_trace, "**** ERROR **** Could not get socket: %s!\n",
	   strerror (errno));
    db->mirror_fd = -1;
    return(0);
  }

  /* non blocking connect */
  trace (NORM, default_trace, "(%s) Connecting to Mirror %s:%d\n",
         db->name, prefix_toa (db->mirror_prefix), db->mirror_port);
  n = nonblock_connect (default_trace, db->mirror_prefix, 
			 db->mirror_port, db->mirror_fd);

  if (n !=1 ) {
    trace (NORM, default_trace, "*ERROR* (%s) Connect to mirror %s:%d failed\n",
           db->name, prefix_toa (db->mirror_prefix), db->mirror_port);
    close (db->mirror_fd);
    db->mirror_fd = -1;
    return (-1);
  }
  
  strcpy (name, db->name);
  convert_toupper(name);
  if (last == 0)
    sprintf (tmp, "-g %s:1:%ld-LAST\n", name, db->serial_number+1);
  else
    sprintf (tmp, "-g %s:1:%ld-%d\n", name, db->serial_number+1, last);

  trace (NORM, default_trace, "(%s) Requesting mirror: %s", 
	 db->name, tmp);

  sprintf (logfile, "%s/.%s.mirror", IRR.database_dir, db->name);
  trace (NORM, default_trace, " (%s) Saving updates to %s\n", 
         db->name, logfile);
  if ((db->mirror_disk_fp = fopen (logfile, "w+")) == NULL) {
    trace (NORM, default_trace, " (%s) Error opening %s (%s)\n",
           db->name, logfile, strerror (errno));
    close (db->mirror_fd);
    db->mirror_fd = -1;
    return(0);
  }

  /* JMH - I think this section of code is redundant.  nonblock_connect
     already checks to see if the connection is ready to be written to */
  /* check if mirror server is ready for writing */
  tv.tv_sec = 1; /* one second is plenty */
  tv.tv_usec = 0;
  FD_ZERO(&write_fds);
  FD_SET(db->mirror_fd, &write_fds);
  ret = select (FD_SETSIZE, NULL, &write_fds, NULL, &tv);
  if (ret <= 0) {
    trace (NORM, default_trace, " (%s) Error writing request (timeout) to %s:%d\n", 
	   db->name, prefix_toa (db->mirror_prefix), db->mirror_port);
    close (db->mirror_fd);
    fclose (db->mirror_disk_fp); /* and delete tmp file??? */
    db->mirror_fd = -1;	   
    return 0;
  }

  /* write request to mirror server */
  if (write (db->mirror_fd, tmp, strlen (tmp)) != strlen (tmp)) {
    trace (NORM, default_trace, " (%s) Error writing request (write failed) to %s:%d\n", 
	   db->name, prefix_toa (db->mirror_prefix), db->mirror_port);
    close (db->mirror_fd);
    fclose (db->mirror_disk_fp); /* and delete tmp file??? */
    db->mirror_fd = -1;	   
    return 0;
  }

  /* reset statistics */
  db->num_changes = 0;
  for (i=0; i<= IRR_MAX_KEYS; i++) {
    db->num_objects_deleted[i] = 0;
    db->num_objects_changed[i] = 0;
    db->num_objects_notfound[i] = 0;
  }
  db->mirror_update_size = 0;

  /* save when we started, so we can check for a timeout */
  db->mirror_started = time (NULL);

  trace (TRACE, default_trace, " (%s) About to perform a select_add_fd\n",
         db->name);
  select_add_fd (db->mirror_fd, SELECT_READ, (void_fn_t) mirror_read_data, db);
  return (1);
}

static int mirror_read_data (irr_database_t *database) {
  struct timeval	tv;
  fd_set		read_fds;
  char			tmp[MIRROR_BUFFER];
  int			ret, n, valid_start_line_ret;

  tv.tv_sec = 0;
  tv.tv_usec = 0;

  FD_ZERO(&read_fds);
  FD_SET(database->mirror_fd, &read_fds);

  while (1) {
    ret = select (FD_SETSIZE, &read_fds, NULL, NULL, &tv);
    if (ret <= 0) {
      select_enable_fd (database->mirror_fd);
      return 0;
    }

    /* If read = 0, connection is close and we are done */
    if ((n = read (database->mirror_fd, tmp, MIRROR_BUFFER)) <= 0) break;

    database->mirror_update_size += n;
    if (fwrite (tmp, 1, n, database->mirror_disk_fp) != n) {
      trace (NORM, default_trace, " (%s) Mirroring Error: Failed on writing mirroring data to disk.\n", database->name);
      fclose (database->mirror_disk_fp);
      select_delete_fd (database->mirror_fd);
      database->mirror_fd = -1;

      return (-1);
    }
    select_enable_fd (database->mirror_fd);
  }

  trace (NORM, default_trace, " (%s) Read %d bytes\n", 
         database->name, database->mirror_update_size);

  if ((valid_start_line_ret = valid_start_line (database,
       database->mirror_disk_fp, 
       &database->new_serial_number)) > 0) {
    irr_update_lock (database);

    if (scan_irr_file (database, "mirror", 2, database->mirror_disk_fp) != NULL) {
      trace (NORM, default_trace, 
	     " (%s) Mirroring Error: Serial number unchanged: %d\n",
	     database->name, database->serial_number);
      irr_update_unlock (database);
      fclose (database->mirror_disk_fp);
      select_delete_fd (database->mirror_fd);
      database->mirror_fd = -1;
      return (-1);
    }
    
    select_delete_fd (database->mirror_fd);
    database->mirror_fd = -1;
    database->mirror_error_message[0] = '\0';
    trace (NORM, default_trace, 
	   " (%s) Serial number now %d, Mirroring header said: %d\n", 
	   database->name, database->serial_number, database->new_serial_number);
    
    irr_update_unlock (database);
    database->last_mirrored = time (NULL);

    trace (NORM, default_trace, " (%s) Route Objects    %4d changed, %4d deleted\n",
	   database->name,
	   database->num_objects_changed[ROUTE], 
	   database->num_objects_deleted[ROUTE]);
    trace (NORM, default_trace, " (%s) AutNum Objects   %4d changed, %4d deleted\n",
	   database->name,
	   database->num_objects_changed[AUT_NUM], 
	   database->num_objects_deleted[AUT_NUM]);
  }
  else {
     if (valid_start_line_ret < 0) {
       trace (NORM, default_trace, " (%s) Mirroring failed... no valid START line\n", database->name);
     }
     select_delete_fd (database->mirror_fd);
     database->mirror_fd = -1;
  }
  fclose (database->mirror_disk_fp);
  
  return (1);
}

/*
 * valid_start_line
 *
 * Returns 1 if a valid start line is found and sets serial_num to the
 * last number in the %START line
 * Returns 0 if only blank lines and comments (start with %) are found
 * (no updates)
 * Returns -1 if invalid tokens are before a start line.
 *
 */

int valid_start_line (irr_database_t *database, FILE *fp, u_long *serial_num) {
  char buffer[BUFSIZE], *cp, *last;
  enum STATES state = BLANK_LINE, save_state;
  int  ret = -1;

  fseek (fp, 0L, SEEK_SET);

  while (state != DB_EOF) {
    cp = fgets (buffer, sizeof (buffer) - 1, fp);

    state = get_state (buffer, cp, state, &save_state);
    /* dump comment lines and lines that exceed the buffer size */
    if (state == OVRFLW     || 
	state == OVRFLW_END ||
	state == COMMENT    ||
	state == DB_EOF)
      continue;
    
    if (state == START_F) {
      if (!strncmp ("%START Version: 1", buffer, 17)) {
	strtok_r (buffer, "-", &last);
	if ((cp = strtok_r (NULL, "-", &last)) != NULL) {
	  *serial_num = atol (cp);
	  trace (NORM, default_trace," (%s) Mirror SERIAL Last-(%d)\n", database->name, *serial_num);
	  ret = 1;
	  break;
	}
      }
      else if (buffer[0] == '%') {
	/* Guarantee newline termination */
        if (strlen(buffer) == (sizeof(buffer) - 1))
	  buffer[sizeof(buffer) - 1] = '\n';

        /* Deal with RIPE error messages here */
	/* % ERROR: 2: Invalid range: 11747158-11747157 don't exist
	   We're likely to get this one if we're up to date */
	if (!strncasecmp (buffer, "% ERROR: 2", 10)) {
	  trace (NORM, default_trace, " (%s) Up to date (or requesting bogus range)\n", database->name);
	  ret = 0;
	  break;
	}

	/* % ERROR: 4: Invalid range: serial(s) 1-11532083 don't exist 
	   We're likely to get this if we're starting from scratch */
	else if (!strncasecmp (buffer, "% ERROR: 4", 10)) {
	  trace (NORM, default_trace, " (%s) ERROR: Requesting out of range mirror:\n", database->name);
	  trace (NORM, default_trace, " (%s) %s", database->name, buffer);
	  ret = -1;
	  break;
	}

	/* % ERROR: 3: Invalid range: serial(s) 11747145-99999999999 don't exist 
	   We're getting this one from an explicit request for stuff past LAST */
	else if (!strncasecmp (buffer, "% ERROR: 3", 10)) {
	  trace (NORM, default_trace, " (%s) ERROR: Requesting out of range mirror:\n", database->name);
	  trace (NORM, default_trace, " (%s) %s", database->name, buffer);
	  ret = -1;
	  break;
	}

	/* Deal with IRRd error messages here */
	/* % ERROR: serials (1 - 8206) don't exist!
	   % ERROR: range error 'from > to' (99999999 > 20084) */
        else if (!strncmp (buffer, "% ERROR: range error", 20) ||
	         !strncmp (buffer, "% ERROR: serials", 16)) {
	  trace (NORM, default_trace, " (%s) ERROR: Requesting out of range mirror:\n", database->name);
	  trace (NORM, default_trace, " (%s) %s", database->name, buffer);
	  ret = -1;
	  break;
        }

	/* If we still have an ERROR line here, since its the first line of
	   a stream, we'll consider it a fatal error.  This serves as a
	   catchall and will specifically deal with not being in RIPE 
	   mirroring ACL's. */
	else if (!strncmp (buffer, "% ERROR", 7)) {
	  trace (NORM, default_trace, " (%s) ERROR: Received error in response to mirror request:\n", database->name);
	  trace (NORM, default_trace, " (%s) %s", database->name, buffer);
	  ret = -1;
	  break;
	}

	/* LAST is special.
	   If we ask from the last serial by number, we get this:
	   % ERROR: range error 'from > to' (20085 > 20084)

	   And we ONLY get this if we ask for LAST+1-LAST
	   % Warning: there are no newer updates available! */
        else if (!strncmp (buffer, "% Warning: there are no newer updates available!", 48)){
	  trace (NORM, default_trace, " (%s) Up to date (or requesting bogus range)\n", database->name);
	  ret = 0;
	  break;
	}

        continue;
      }
      else
	break;
    }
  }

  if (state == DB_EOF) {
    trace (NORM, default_trace, " (%s) No updates found in mirror stream.\n",
           database->name);
    return (0);
  }
  else if ((state == START_F) && (ret != 1) && (*buffer != '%')) {
    trace (NORM, default_trace, " (%s) Invalid mirror header line: %s\n",
           database->name, buffer);
    strncpy (database->mirror_error_message, buffer, 60);
    database->mirror_error_message[60] = '\0';
  }

  return (ret);
}

void irr_mirror_timer (mtimer_t *timer, irr_database_t *db) {
  request_mirror (db, NULL, 0);
}

int irr_service_mirror_request (irr_connection_t *irr, char *command) {
  char buffer[BUFSIZE];
  irr_database_t *database;
  u_long oldestserial, currentserial, first_in_new;
  u_long from, to;
  int old_journal_exists, new_journal_exists;
  int ret_code = 1;
  char name[BUFSIZE], buffer1[BUFSIZE];

  /* Parse a valid -g mirror request line and set request "from" - "to" */
  if (sscanf (command, " %[^:]:1:%[^-]-%s", name, buffer1, buffer) != 3) {
    sprintf (buffer, "\n\n\n%% ERROR: syntax error in -g command: (%s)\n", command);
    irr_write_nobuffer (irr, buffer, strlen (buffer));
    return (-1);
  }
  else {
    if (convert_to_lu (buffer1, &from) < 0) {
      sprintf (buffer, "\n\n\n%% ERROR: syntax error in 'from' value (%s)\n", buffer1);
      irr_write_nobuffer (irr, buffer, strlen (buffer));
      return (-1);
    }
    if (strncasecmp (buffer, "LAST", 4) != 0 && 
        convert_to_lu (buffer, &to) < 0) {
      sprintf (buffer1, "\n\n\n%% ERROR: syntax error in 'to' value (%s)\n", buffer);
      irr_write_nobuffer (irr, buffer1, strlen (buffer1));
      return (-1);
    }
  }
  
  /* See if the db name is one we know about and is mirrored */
  if ((database = find_database (name)) == NULL) {
    sprintf (buffer, "\n\n\n%% ERROR: Unknown db (%s) in mirror request\n", name);
    irr_write_nobuffer (irr, buffer, strlen (buffer));
    return (-1);
  }
  else if (database->mirror_prefix == NULL &&
	   !(database->flags & IRR_AUTHORITATIVE)) {
    sprintf (buffer, "\n\n\n%% ERROR: Database (%s) is a non-mirrored db!\n", database->name);
    irr_write_nobuffer (irr, buffer, strlen (buffer));
    return (-1);
  }
  
  if (get_current_serial (name, &currentserial) < 0) {
    if (database->db_syntax != EMPTY_DB) {
      sprintf (buffer, "\n\n\n%% ERROR: (%s).CURRENTSERIAL file missing or corrupted!\n", name);
      irr_write_nobuffer (irr, buffer, strlen (buffer));
      return (-1);
    }
    /* DB is empty so there are no updates to mirror */
    currentserial = 0;
  }

  if (currentserial == 0) {
    sprintf (buffer, "\n\n\n%%Warning: No updates yet, journal file is empty!\n");
    irr_write_nobuffer (irr, buffer, strlen (buffer));
    return (-1);
  }
  
  if (!strncasecmp (buffer, "LAST", 4)) {
    to = currentserial;
    if (from == (currentserial + 1)) {
      sprintf (buffer, "\n\n\n%% Warning: there are no newer updates available!\n");
      irr_write_nobuffer (irr, buffer, strlen (buffer));
      return (-1);
    }
  }

  trace (NORM, default_trace, "Processing mirror request from %s for %s %d - %d\n",
	 prefix_toa (irr->from), database->name, from, to);

  /* check acls */
  if (!apply_access_list (database->mirror_access_list, irr->from)) {
    trace (NORM | INFO, default_trace, "IRR DENIED to %s\n",
	   prefix_toa (irr->from));
    irr_send_error (irr, NULL);
    return (-1);
  }

  irr_lock (database);  /* lock to build buffer with mirror data */

  /* find the oldest serial we have in *.JOURNAL.1 */
  old_journal_exists = find_oldest_serial (database->name, JOURNAL_OLD, &oldestserial);

  /* now find the first value in the *.JOURNAL file */
  if ((new_journal_exists = 
      find_oldest_serial (database->name, JOURNAL_NEW, &first_in_new)) == 0) {
    irr_unlock (database);
    sprintf (buffer, "\n\n%% WARN: No serials to mirror yet or the first serial is corrupted!\n" );
    irr_write_nobuffer (irr, buffer, strlen (buffer));
    return (-1);
  }

  /* oldestserial is the first in *.JOURNAL if *.JOURNAL.1 doesn't exist */
  if (!old_journal_exists)
    oldestserial = first_in_new;

  /* check and see if there are any "from-to" range errors in the request */
  if (from > to) {
    irr_unlock (database);
    sprintf (buffer, "\n\n%% ERROR: range error 'from > to' (%lu > %lu)\n", from, to);
    irr_write_nobuffer (irr, buffer, strlen (buffer));
    return (-1);
  }
  
  if (from < oldestserial) {
    irr_unlock (database);
    sprintf (buffer, "\n\n%% ERROR: serials (%lu - %lu) don't exist!\n", from, oldestserial - 1);
    irr_write_nobuffer (irr, buffer, strlen (buffer));
    return (-1);
  }
  
  if (to > currentserial) {
    irr_unlock (database);
    sprintf (buffer, "\n\n%% ERROR: serials (%lu - %lu) don't exist!\n", currentserial + 1, to);
    irr_write_nobuffer (irr, buffer, strlen (buffer));
    return (-1);
  }

  if (old_journal_exists && from < first_in_new) {
    ret_code = dump_serial_updates (irr, database->name, JOURNAL_OLD, from, to);
  }

  if (ret_code < 0) {
    irr_unlock (database);
    return (-1);
  }
  
  if (to >= first_in_new) {
    ret_code = dump_serial_updates (irr, database->name, JOURNAL_NEW, from, to);
  }

  if (ret_code < 0) {
    irr_unlock (database);
    return (-1);
  }

  irr_unlock (database);

  sprintf (buffer, "%%START Version: 1 %s %lu-%lu\n\n", database->name, from, to);

  irr_write_nobuffer(irr, buffer, strlen(buffer));

  if (irr->scheduled_for_deletion) {
    LL_Destroy (irr->ll_final_answer);
    trace (NORM, default_trace, "Mirror send %%START error\n");
    return (-1);
  }

  irr_write_buffer_flush(irr);  /* attempt to send our mirror data */

  if (irr->scheduled_for_deletion) {
    trace (NORM, default_trace, "Mirror send DATA error\n");
    return (-1);
  }

  sprintf (buffer, "%%END %s\n", database->name);
  /* traditional perl clients only accept 'DB name'
   * in upper case
   */
  convert_toupper (buffer);

  irr_write_nobuffer(irr, buffer, strlen(buffer));

  if (irr->scheduled_for_deletion) {
    trace (NORM, default_trace, "Mirror send %%END error\n");
    return -1;
  }
  return (0);
}

int dump_serial_updates (irr_connection_t *irr, char * dbname, int journal_ext, 
			 u_long from, u_long to) {
  char buf[BUFSIZE], file[BUFSIZE];
  u_long serial;
  int ret_code = 1;
  FILE *journal_fp;

  make_journal_name (dbname, journal_ext, file);
  
  if ((journal_fp = fopen (file, "r")) == NULL) 
    return (-1);

  /* find the correct spot in the journal file, then pump the updates onto the socket */
  while (fgets (buf, BUFSIZE - 1, journal_fp) != NULL) {
    if (sscanf (buf, "%% SERIAL %lu", &serial) == 1) {
      if (serial >= from) {
	ret_code = build_update (file, irr, journal_fp, serial, to);
	break;
      }
    }
  }
  fclose (journal_fp);

  return ret_code;
}

int build_update (char *file, irr_connection_t *irr, FILE *fp, u_long curr_serial, 
			 u_long to) {
  char buf[BUFSIZE];
  u_long chk_serial;

  while ((fgets (buf, BUFSIZE - 1, fp) != NULL) &&
	 (curr_serial <= to)) {

    if (sscanf (buf, "%% SERIAL %lu", &chk_serial) == 1) {
      if (++curr_serial != chk_serial) {
	trace (NORM, default_trace, "ERROR: Skipped sequence mirror update, should be (%lu) (%s) says (%lu)\n", curr_serial, file, chk_serial);
	return -1;
      }
      continue;
    }
    
    irr_write (irr, buf, strlen (buf));
  }
  return 1;
}
