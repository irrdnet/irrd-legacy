/*
 * $Id: mirror.c,v 1.10 2002/10/17 20:02:31 ljb Exp $
 * originally Id: mirror.c,v 1.26 1998/08/03 21:55:23 gerald Exp 
 */

#include <stdio.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <ctype.h>

#include "mrt.h"
#include "trace.h"
#include "config_file.h"
#include "select.h"
#include "irrd.h"
#include "irrd_prototypes.h"

static int mirror_read_data ();
int dump_serial_updates (irr_connection_t *irr, irr_database_t *database, int journal_ext, uint32_t protocol_num, uint32_t from, uint32_t to);
int valid_start_line (irr_database_t *db, FILE *fp, uint32_t *serial_num);

int request_mirror (irr_database_t *db, uii_connection_t *uii, uint32_t last) {
  char tmp[BUFSIZE], name[BUFSIZE], logfile[BUFSIZE];
  struct timeval	tv;
  fd_set		write_fds;
  int			n, i, ret;

  if (db->db_fp == NULL) {
    trace (ERROR, default_trace, 
	   "Aborting mirror -- we don't have an open file pointer for %s\n",
	   db->name);
    return (-1);
  }

  if (last > 0 &&
     (db->serial_number + 1) > last) {
    trace (ERROR, default_trace,
	   "user supplied serial # last (%u) is <= current serial (%u)\n", 
	   last, db->serial_number);
    return (-1);
  }

  /* 
   * Check if already mirroring. 
   * If so, check if we want to time out old attempt 
   */
  if (db->mirror_fd != -1) {
    if ( (time (NULL) - db->mirror_started) < MIRROR_TIMEOUT) {
      trace (ERROR, default_trace, "(%s) already mirroring...!\n",
	     db->name);
      return (-1);
    }

    trace (ERROR, default_trace, "*** TIMING OUT old mirror attempt to %s\n",
	   db->name);
    select_delete_fd (db->mirror_fd);
    fclose (db->mirror_disk_fp);
    db->mirror_disk_fp = NULL;
    db->mirror_fd = -1;
    strcpy (db->mirror_error_message, "Mirroring timed out...");
    return (-1);
  }

  db->mirror_error_message[0] = '\0';

  if ((db->mirror_fd = socket (AF_INET, SOCK_STREAM, 0)) < 0) {
    trace (ERROR, default_trace, "Could not get socket: %s!\n",
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
    trace (ERROR, default_trace, "(%s) Connect to mirror %s:%d failed\n",
           db->name, prefix_toa (db->mirror_prefix), db->mirror_port);
    close (db->mirror_fd);
    db->mirror_fd = -1;
    return (-1);
  }
  
  strcpy (name, db->name);
  convert_toupper(name);
  if (last == 0)
    sprintf (tmp, "-g %s:%d:%u-LAST\n", name, db->mirror_protocol, db->serial_number+1);
  else
    sprintf (tmp, "-g %s:%d:%u-%u\n", name, db->mirror_protocol, db->serial_number+1, last);

  trace (NORM, default_trace, "(%s) Requesting mirror: %s", 
	 db->name, tmp);

  sprintf (logfile, "%s/.%s.mirror", IRR.database_dir, db->name);
  trace (NORM, default_trace, "(%s) Saving updates to %s\n", 
         db->name, logfile);
  if ((db->mirror_disk_fp = fopen (logfile, "w+")) == NULL) {
    trace (ERROR, default_trace, "(%s) Error opening %s (%s)\n",
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
    trace (ERROR, default_trace, "(%s) Error writing request (timeout) to %s:%d\n", 
	   db->name, prefix_toa (db->mirror_prefix), db->mirror_port);
    close (db->mirror_fd);
    fclose (db->mirror_disk_fp); /* and delete tmp file??? */
    db->mirror_fd = -1;	   
    return 0;
  }

  /* write request to mirror server */
  if (write (db->mirror_fd, tmp, strlen (tmp)) != strlen (tmp)) {
    trace (ERROR, default_trace, "(%s) Error writing request (write failed) to %s:%d\n", 
	   db->name, prefix_toa (db->mirror_prefix), db->mirror_port);
    close (db->mirror_fd);
    fclose (db->mirror_disk_fp); /* and delete tmp file??? */
    db->mirror_fd = -1;	   
    return 0;
  }

  /* reset statistics */
  db->num_changes = 0;
  for (i=0; i<IRR_MAX_CLASS_KEYS; i++) {
    db->num_objects_deleted[i] = 0;
    db->num_objects_changed[i] = 0;
    db->num_objects_notfound[i] = 0;
  }
  db->mirror_update_size = 0;

  /* save when we started, so we can check for a timeout */
  db->mirror_started = time (NULL);

  trace (TRACE, default_trace, "(%s) About to perform a select_add_fd\n", db->name);
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
      trace (ERROR, default_trace, "(%s): Failed on writing mirroring data to disk.\n", database->name);
      fclose (database->mirror_disk_fp);
      select_delete_fd (database->mirror_fd);
      database->mirror_fd = -1;

      return (-1);
    }
    select_enable_fd (database->mirror_fd);
  }

  trace (NORM, default_trace, "(%s) Read %d bytes\n", 
         database->name, database->mirror_update_size);

  if ((valid_start_line_ret = valid_start_line (database,
       database->mirror_disk_fp, 
       &database->new_serial_number)) > 0) {
    irr_update_lock (database);

    if (scan_irr_file (database, "mirror", 2, database->mirror_disk_fp) != NULL) {
      trace (ERROR, default_trace, 
	     "(%s) Mirroring Error: Serial number unchanged: %d\n",
	     database->name, database->serial_number);
      irr_update_unlock (database);
      fclose (database->mirror_disk_fp);
      select_delete_fd (database->mirror_fd);
      database->mirror_fd = -1;
      return (-1);
    }
    
    irr_update_unlock (database);
    fclose (database->mirror_disk_fp);
    select_delete_fd (database->mirror_fd);
    database->mirror_fd = -1;
    database->mirror_error_message[0] = '\0';
    database->last_mirrored = time (NULL);
    trace (NORM, default_trace, 
	   "(%s) Serial number now %d, Mirroring header said: %d\n", 
	   database->name, database->serial_number, database->new_serial_number);

    trace (NORM, default_trace, "(%s) Route Objects    %4d changed, %4d deleted\n",
	   database->name,
	   database->num_objects_changed[ROUTE], 
	   database->num_objects_deleted[ROUTE]);
    trace (NORM, default_trace, "(%s) AutNum Objects   %4d changed, %4d deleted\n",
	   database->name,
	   database->num_objects_changed[AUT_NUM], 
	   database->num_objects_deleted[AUT_NUM]);
    return (1);
  }
  fclose (database->mirror_disk_fp);
  select_delete_fd (database->mirror_fd);
  database->mirror_fd = -1;
  if (valid_start_line_ret < 0) {
     trace (ERROR, default_trace, "(%s) Mirroring failed... no valid START line\n", database->name);
  }
  return (-1);
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

int valid_start_line (irr_database_t *database, FILE *fp, uint32_t *serial_num) {
  char buffer[BUFSIZE], *cp, *last;
  enum STATES state = BLANK_LINE, save_state;
  u_long len;
  uint32_t protocol_version;
  int  ret = -1;

  fseek (fp, 0L, SEEK_SET);

  while (state != DB_EOF) {
    cp = fgets (buffer, BUFSIZE, fp);

    len = strlen(buffer);
    state = get_state (cp, len, state, &save_state);
    /* dump comment lines and lines that exceed the buffer size */
    if (state == OVRFLW     || 
	state == OVRFLW_END ||
	state == COMMENT    ||
	state == DB_EOF)
      continue;
    
    if (state == START_F) {
      if (!strncmp ("%START Version:", buffer, 15)) {
        cp = strtok_r (buffer+15, " ", &last);
        if (convert_to_32(cp, &protocol_version) != 1) {
          trace (ERROR, default_trace, " (%s) Error getting protocol: %s", database->name, buffer);
          ret = -1;
          break;
        }
        if (protocol_version != database->mirror_protocol) {
          trace (ERROR, default_trace, " (%s) Mirror protocol incorrect: %d", database->name, protocol_version);
          ret = -1;
          break;
        }
        cp = strtok_r (NULL, " ", &last);
        if (strcasecmp(cp, database->name) != 0) {
          trace (ERROR, default_trace, " (%s) Database name mismatch: %s", database->name, cp);
          ret = -1;
          break;
        }
        if ((cp = strtok_r (NULL, "-", &last)) != NULL) {
          *serial_num = atol (cp);
          trace (NORM, default_trace,"(%s) Mirror SERIAL First-(%d)\n", database->name, *serial_num);
        } else {
          trace (ERROR, default_trace, " (%s) Database first serial number scan error: %s", database->name, cp);
          ret = -1;
          break;
        }
	if ((cp = strtok_r (NULL, "-", &last)) != NULL) {
	  *serial_num = atol (cp);
	  trace (NORM, default_trace,"(%s) Mirror SERIAL Last-(%d)\n", database->name, *serial_num);
	  ret = 1;
	  break;
        } else {
          trace (ERROR, default_trace, " (%s) Database last serial number scan error: %s", database->name, cp);
          ret = -1;
          break;
        }
      }
      else if (buffer[0] == '%') {
	/* Guarantee newline termination */
        if (len == (sizeof(buffer) - 1))
	  buffer[sizeof(buffer) - 1] = '\n';

        /* Deal with RIPE error messages here */
	/* % ERROR: 2: Invalid range: 11747158-11747157 don't exist
	   We're likely to get this one if we're up to date */
	if (!strncasecmp (buffer, "% ERROR: 2", 10)) {
	  trace (NORM, default_trace, "(%s) Up to date (or requesting bogus range)\n", database->name);
	  ret = 0;
	  break;
	}

	/* % ERROR: 4: Invalid range: serial(s) 1-11532083 don't exist 
	   We're likely to get this if we're starting from scratch */
	else if (!strncasecmp (buffer, "% ERROR: 4", 10)) {
	  trace (ERROR, default_trace, " (%s) Requesting out of range mirror:\n%s", database->name, buffer);
	  ret = -1;
	  break;
	}

	/* % ERROR: 3: Invalid range: serial(s) 11747145-99999999999 don't exist 
	   We're getting this one from an explicit request for stuff past LAST */
	else if (!strncasecmp (buffer, "% ERROR: 3", 10)) {
	  trace (ERROR, default_trace, " (%s) Requesting out of range mirror:\n%s", database->name, buffer);
	  ret = -1;
	  break;
	}

	/* Deal with IRRd error messages here */
	/* % ERROR: serials (1 - 8206) don't exist!
	   % ERROR: range error 'from > to' (99999999 > 20084) */
        else if (!strncmp (buffer, "% ERROR: range error", 20) ||
	         !strncmp (buffer, "% ERROR: serials", 16)) {
	  trace (ERROR, default_trace, " (%s) Requesting out of range mirror:\n%s", database->name, buffer);
	  ret = -1;
	  break;
        }

	/* If we still have an ERROR line here, since its the first line of
	   a stream, we'll consider it a fatal error.  This serves as a
	   catchall and will specifically deal with not being in RIPE 
	   mirroring ACL's. */
	else if (!strncmp (buffer, "% ERROR", 7)) {
	  trace (ERROR, default_trace, " (%s) Received error in response to mirror request:\n%s", database->name, buffer);
	  ret = -1;
	  break;
	}

	/* LAST is special.
	   If we ask from the last serial by number, we get this:
	   % ERROR: range error 'from > to' (20085 > 20084)

	   And we ONLY get this if we ask for LAST+1-LAST
	   % Warning: there are no newer updates available! */
        else if (!strncmp (buffer, "% Warning: there are no newer updates available!", 48)){
	  trace (NORM, default_trace, "(%s) Up to date (or requesting bogus range)\n", database->name);
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
    trace (NORM, default_trace, "(%s) No updates found in mirror stream.\n",
           database->name);
    return (0);
  }
  else if ((state == START_F) && (ret != 1) && (*buffer != '%')) {
    trace (ERROR, default_trace, " (%s) Invalid mirror header line: %s\n",
           database->name, buffer);
    strncpy (database->mirror_error_message, buffer, MAX_MIRROR_ERROR_LEN);
    database->mirror_error_message[MAX_MIRROR_ERROR_LEN] = '\0';
  }

  return (ret);
}

void irr_mirror_timer (mtimer_t *timer, irr_database_t *db) {
  request_mirror (db, NULL, 0);
}

int irr_service_mirror_request (irr_connection_t *irr, char *command) {
  char buffer[BUFSIZE];
  irr_database_t *database;
  uint32_t oldestserial, currentserial, first_in_new;
  uint32_t from, to, protocol_num;
  int old_journal_exists, new_journal_exists;
  int ret_code = 1;
  char name[BUFSIZE], version[2], buffer1[BUFSIZE];

  /* Parse a valid -g mirror request line and set request "from" - "to" */
  if (sscanf (command, " %[^:]:%c:%[^-]-%s", name, version, buffer1, buffer) != 4) {
    sprintf (buffer, "\n\n\n%% ERROR: syntax error in -g command: (%s)\n", command);
    irr_write_nobuffer (irr, buffer);
    return (-1);
  }
  else {
    version[1] = 0;
    if (convert_to_32 (version, &protocol_num) < 0) {
      sprintf (buffer, "\n\n\n%% ERROR: syntax error in procotol version value (%s)\n", version);
      irr_write_nobuffer (irr, buffer);
      return (-1);
    }
    if (protocol_num != 1 && protocol_num != 3) {
      sprintf (buffer, "\n\n\n%% ERROR: unsupported procotol version (%d)\n", protocol_num);
      irr_write_nobuffer (irr, buffer);
      return (-1);
    }
    if (convert_to_32 (buffer1, &from) < 0) {
      sprintf (buffer, "\n\n\n%% ERROR: syntax error in 'from' value (%s)\n", buffer1);
      irr_write_nobuffer (irr, buffer);
      return (-1);
    }
    if (strncasecmp (buffer, "LAST", 4) != 0 && 
        convert_to_32 (buffer, &to) < 0) {
      sprintf (buffer1, "\n\n\n%% ERROR: syntax error in 'to' value (%s)\n", buffer);
      irr_write_nobuffer (irr, buffer1);
      return (-1);
    }
  }
  
  /* See if the db name is one we know about and is mirrored */
  if ((database = find_database (name)) == NULL) {
    sprintf (buffer, "\n\n\n%% ERROR: Unknown db (%s) in mirror request\n", name);
    irr_write_nobuffer (irr, buffer);
    return (-1);
  }
  else if (database->mirror_prefix == NULL &&
	   !(database->flags & IRR_AUTHORITATIVE)) {
    sprintf (buffer, "\n\n\n%% ERROR: Database (%s) is a non-mirrored db!\n", database->name);
    irr_write_nobuffer (irr, buffer);
    return (-1);
  }
  
  if (get_current_serial (name, &currentserial) < 0) {
    if (database->bytes != 0) {
      sprintf (buffer, "\n\n\n%% ERROR: (%s).CURRENTSERIAL file missing or corrupted!\n", name);
      irr_write_nobuffer (irr, buffer);
      return (-1);
    }
    /* DB is empty so there are no updates to mirror */
    currentserial = 0;
  }

  if (currentserial == 0) {
    sprintf (buffer, "\n\n\n%% Warning: No updates yet, journal file is empty!\n");
    irr_write_nobuffer (irr, buffer);
    return (-1);
  }
  
  if (!strncasecmp (buffer, "LAST", 4)) {
    to = currentserial;
    if (from == (currentserial + 1)) {
      sprintf (buffer, "\n\n\n%% Warning: there are no newer updates available!\n");
      irr_write_nobuffer (irr, buffer);
      return (-1);
    }
  }

  trace (NORM, default_trace, "Processing mirror request from %s for %s %d - %d\n",
	 prefix_toa (irr->from), database->name, from, to);

  /* check acls */
  if (!apply_access_list (database->mirror_access_list, irr->from)) {
    trace (NORM | INFO, default_trace, "mirror access denied to %s\n",
	   prefix_toa (irr->from));
    sprintf (buffer, "\n\n\n%% ERROR: mirror access denied\n");
    irr_write_nobuffer (irr, buffer);
    return (-1);
  }

  irr_lock (database);  /* lock to build buffer with mirror data */

  /* find the oldest serial we have in *.JOURNAL.1 */
  old_journal_exists = find_oldest_serial (database->name, JOURNAL_OLD, &oldestserial);

  /* now find the first value in the *.JOURNAL file */
  if ((new_journal_exists = 
      find_oldest_serial (database->name, JOURNAL_NEW, &first_in_new)) == 0) {
    irr_unlock (database);
    sprintf (buffer, "\n\n%% Warning: No serials to mirror yet or the first serial is corrupted!\n" );
    irr_write_nobuffer (irr, buffer);
    return (-1);
  }

  /* oldestserial is the first in *.JOURNAL if *.JOURNAL.1 doesn't exist */
  if (!old_journal_exists)
    oldestserial = first_in_new;

  /* check and see if there are any "from-to" range errors in the request */
  if (from > to) {
    irr_unlock (database);
    sprintf (buffer, "\n\n%% ERROR: range error 'from > to' (%u > %u)\n", from, to);
    irr_write_nobuffer (irr, buffer);
    return (-1);
  }
  
  if (from < oldestserial) {
    irr_unlock (database);
    sprintf (buffer, "\n\n%% ERROR: serials (%u - %u) don't exist!\n", from, oldestserial - 1);
    irr_write_nobuffer (irr, buffer);
    return (-1);
  }
  
  if (to > currentserial) {
    irr_unlock (database);
    sprintf (buffer, "\n\n%% ERROR: serials (%u - %u) don't exist!\n", currentserial + 1, to);
    irr_write_nobuffer (irr, buffer);
    return (-1);
  }

  if (old_journal_exists && from < first_in_new) {
    ret_code = dump_serial_updates (irr, database, JOURNAL_OLD, protocol_num, from, to);
  }

  if (ret_code < 0) {
    irr_unlock (database);
    return (-1);
  }
  
  if (to >= first_in_new) {
    ret_code = dump_serial_updates (irr, database, JOURNAL_NEW, protocol_num, from, to);
  }

  if (ret_code < 0) {
    irr_unlock (database);
    return (-1);
  }

  irr_unlock (database);

  sprintf (buffer, "%%START Version: %d %s %u-%u\n\n", protocol_num, database->name, from, to);

  irr_write_nobuffer(irr, buffer);

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

  irr_write_nobuffer(irr, buffer);

  if (irr->scheduled_for_deletion) {
    trace (NORM, default_trace, "Mirror send %%END error\n");
    return -1;
  }
  return (0);
}

int dump_serial_updates (irr_connection_t *irr, irr_database_t *database, int journal_ext, uint32_t protocol_num, uint32_t from, uint32_t to) {
  char buf[BUFSIZE], file[256];
  uint32_t serial = 0, chk_serial = 0;
  int overflownow, overflowlast = 0;
  FILE *journal_fp;

  make_journal_name (database->name, journal_ext, file);
  
  if ((journal_fp = fopen (file, "r")) == NULL) 
    return (-1);

  /* find the correct spot in the journal file, then pump the updates onto the socket */
  for (buf[BUFSIZE - 1] = 0xff; fgets(buf, BUFSIZE, journal_fp) != NULL; buf[BUFSIZE - 1] = 0xff) {
    if (buf[BUFSIZE - 1] == 0 && buf[BUFSIZE - 2] != '\n') /* need to check for long lines */
	overflownow = 1;
    else
	overflownow = 0;
    if (overflowlast) {	/* if overflow state, don't check for serial num */
	overflowlast = overflownow;
	if (serial >= from)
          irr_write (irr, buf, strlen(buf));
	continue;
    }
    overflowlast = overflownow;
    if (!strncmp(buf, "% SERIAL", 8)) {
      serial = atol(buf + 9);
      if (chk_serial == 0)
        chk_serial = serial;
      if (chk_serial++ != serial) {
        if (protocol_num == 3 && serial >= chk_serial) { /* mirror protocol 3 may skip serial */
          chk_serial = serial+1;
        } else {
          trace (ERROR, default_trace, "Skipped sequence mirror update, should be (%u) (%s) says (%u)\n", chk_serial - 1, file, serial);
          fclose (journal_fp);
          return (-1);
        }
      }
      if (serial > to)
        break;
      if (serial < from)
      continue;
      if (fgets(buf,BUFSIZE,journal_fp) != NULL) {
        if (strncmp(buf, "ADD", 3) && strncmp(buf, "DEL", 3)) {
          trace (ERROR, default_trace, "Improper mirror update, should be ADD or DEL but is:%s\n", buf);
          fclose (journal_fp);
          return (-1);
        }
        if (protocol_num == 3) {
          sprintf(buf + 3, " %u\n", serial);
        }
        irr_write (irr, buf, strlen(buf));
        continue;
      } else {
          trace (ERROR, default_trace, "Error reading update type: %s", buf);
          fclose (journal_fp);
          return (-1);
      }
    }
    if (serial < from)
      continue;
    if (!strncasecmp(buf, "auth:", 5)) {
      if (database->cryptpw_access_list != 0 && 
          !apply_access_list(database->cryptpw_access_list, irr->from) ) {
	  scrub_cryptpw(buf);
          scrub_md5pw(buf);
      }
    } 
    irr_write (irr, buf, strlen(buf));
  }
  fclose (journal_fp);
  return 1;
}
