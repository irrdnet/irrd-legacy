/*
 * $Id: database.c,v 1.17 2002/10/17 20:02:29 ljb Exp $
 * originally Id: database.c,v 1.48 1998/07/30 20:48:29 labovit Exp 
 */

#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <dirent.h>
#include <fcntl.h>
#include <pipeline_defs.h>
#include <atomic_ops.h>
#include <glib.h>

#include "mrt.h"
#include "trace.h"
#include "config_file.h"
#include "irrd.h"

/* !B/reload support functions */
static int reopen_DB        (irr_database_t *, char *);
static int reopen_JOURNAL   (irr_database_t *, char *);
static int replace_cache_db (irr_database_t *, uii_connection_t *, char *);

/* bootstrap load support functions */
static int ftp_db_fetch     (irr_database_t *);
static int fetch_remote_db  (irr_database_t *, char *);

void munge_buffer (char *buffer, irr_database_t *irr_database);
int irr_check_serial_vs_journal (irr_database_t *database);

/* database_clear
 * delete all indicies associated with a database 
 */
void database_clear (irr_database_t *db) {
  int i;

  if (db == NULL) {
    trace (ERROR, default_trace, "*** NULL DATABASE!!! ****\n");
    return;
  }

  trace (NORM, default_trace, "Clearing out database %s\n", db->name);

  db->bytes = 0;
  for (i=0; i< IRR_MAX_KEYS; i++) 
    db->num_objects[i] = 0;
  
  radix_flush (db->radix_v4);
  radix_flush (db->radix_v6);

  if (db->hash)
    g_hash_table_remove_all(db->hash);

  if (db->hash_spec)
    g_hash_table_remove_all(db->hash_spec);

  db->radix_v4 = New_Radix (32);
  db->radix_v6 = New_Radix (128);
  fclose (db->db_fp);

  db->db_fp = NULL;
}


/* Control the process of reloading database (name).
 *
 * Can be invoked by irrdcacher via the !B command or from the
 * uii via the 'reload <db name>' command or as part of a atomic
 * DB rollback operation.
 *
 * The reload process is carried out atomically and is resistent
 * to disk crashes.
 *
 * If (tmp_dir) is non-null then routine atomically replaces the
 * database in (tmp_dir) with the corresponding cache DB.  This is
 * how the !B would invoke this command.  Otherwise the 'reload <db name>' 
 * is assumed which means to rebuild the indices.  If both (uii) and
 * (tmp_dir) are NULL then the function is called as part of a DB rollback
 * operation.
 *
 * The function assumes the name of the new DB is the same as the
 * corresponding DB cache name.
 *
 * Input:
 *  -name of the DB to be reloaded (name)
 *  -data struct for communicating with UII users (uii)
 *  -(optional) directory to find the replacement DB (tmp_dir) 
 *
 * Return:
 *  -1 if there were no errors
 *  -0 if an error occured.
 */
int irr_reload_database (char *name, uii_connection_t *uii, char *tmp_dir) {
  irr_database_t *database;

  /* do we know of this DB ? */
  database = find_database (name);
  if (database == NULL) {
    if (uii != NULL) 
      uii_send_data (uii, "%s database not found\r\n", name);
    return 0;
  }

  /* if we are called for rollback then we already have the lock */
  if (uii != NULL || tmp_dir != NULL)
    irr_update_lock (database);

  /* atomically move the new DB into our cache area */
  if (tmp_dir != NULL) {
    if (!replace_cache_db (database, uii, tmp_dir)) {
      irr_update_unlock (database);
      trace (ERROR, default_trace, "irr_reload_database (): reload (%s) aborted!\n",
	     database->name);
      if (uii != NULL) 
	uii_send_data (uii, "Operation aborted!\r\n");
      return 0;
    }
  }
    
  if (uii != NULL) 
    uii_send_data (uii, "Clearing out old data...\r\n");
  database_clear (database);

  if (uii != NULL) 
    uii_send_data (uii,"Loading the database and re-building the indicies ...\r\n");

  /* rebuild the indexes and reload the serial file */
  scan_irr_file (database, NULL, 0, NULL);
  scan_irr_serial (database);

  /* TODO - Check return code and do something besides log the trace */
  irr_check_serial_vs_journal (database);

  /* if we are called for rollback then we already have the lock */
  if (uii != NULL || tmp_dir != NULL)
    irr_update_unlock (database);

  if (uii != NULL) 
    uii_send_data (uii, "Successful operation\r\n");

  return 1;
}

/*JW  add a \n to the end of the file to seperate objects */
void append_blank_line (FILE *fp) {
  size_t num;
  long start_offset, tmp_pos;
  char buffer[4];
  
  if ( fp == NULL )
    return;

  start_offset = ftell (fp);
  if (fseek (fp, 0, SEEK_END) < 0) 
    trace (NORM, default_trace, 
	   "** ERROR ** fseek() failed in append_blank_line! [1]");
  
  tmp_pos = ftell (fp);

  if (tmp_pos > 1) {
    if (fseek (fp, (long) tmp_pos - 2, SEEK_SET) < 0) 
      trace (NORM, default_trace, 
	     "** ERROR ** fseek() failed in append_blank_line! [2]");
    if ((num = fread (buffer, 1, 2, fp)) != 2)
      return;

    buffer[2] = '\0';
    if (strcmp (buffer, "\n\n")) {
      buffer[0] = '\n';
      buffer[1] = '\n';
      if (fseek (fp, 0, SEEK_END) < 0) 
	trace (NORM, default_trace, 
	       "** ERROR ** fseek() failed in append_blank_line! [3]");
      fwrite (buffer, 1, 2, fp);
    }
    
    if (fseek (fp, start_offset, SEEK_SET) < 0) 
      trace (NORM, default_trace, 
	     "** ERROR ** fseek() failed in append_blank_line! [4]");
  }
}

/* Load and build the DB indexes and serial numbers on bootstrap.
 *
 * Function will attempt to remote ftp missing non-authoritative DB's if the
 * '-x' command line flag is not set.
 *
 * Function returns the number of empty DB's encountered or a 
 * 'no DB's configured' situation.
 *
 * Input:
 *  -command line option to turn on/off the auto-db fetch feature (db_fetch_flag)
 *   A value of 1 means to turn on the auto-fetch feature.
 *  -flag to indicate verbose output to stdout (verbose)
 *  -uses global DB linked list structure to loop through each DB (IRR.ll_database)
 *
 * Return:
 *  --1 if no DB's were configured in irrd.conf
 *  -0..n the number of empty DB's encounterd (ie, configured but DB has
 *   no objects)
 */
int irr_load_data (int db_fetch_flag, int verbose) {
  int empty_dbs = 0, n = 0, i;
  irr_database_t *db;

  LL_Iterate (IRR.ll_database, db) {

    /* keep a count.  want to know if the user has not configured any DB's */
    n++;

    irr_update_lock (db);

    /* load the DB and build the indicies.  if DB is not in our
     * cache then attempt a remote irrdcacher fetch */
    if (scan_irr_file (db, NULL, 0, NULL) != NULL) {
      i = empty_dbs++;	
	
	/* did the user override our missing DB remote fetch behavior? 
	 * if not then see if we can retrieve the DB from an ftp site */
	if (db_fetch_flag &&
	    !(db->flags & IRR_AUTHORITATIVE))
	  empty_dbs -= ftp_db_fetch (db);

	/* DB is empty and we could not remote fetch */
	if (empty_dbs > i) {
	  trace (NORM, default_trace, "WARNING: Database %s is empty!\n", db->name);
	  if (verbose)
	    printf ("WARNING: Database %s is empty!\n", db->name);
	}
      }
      
    /* initialize the serial file */
    scan_irr_serial (db);
      
    /* TODO - Check return code and do something besides log the trace */
    irr_check_serial_vs_journal (db);
    append_blank_line (db->db_fp);

    irr_update_unlock (db);
  }

  /* signal to caller that the user has not config'd any DB's */
  if (n == 0)
    return -1;

  /* return the number of empty DB's. */
  return empty_dbs;
}

/* the database on disk contains "xx", or deleted objects. Our pointers
 * in memory point to new objects later. This routine creates a new databse
 * and reloads memory with offsets of the new database. The new database file
 * on disk is free of any "xx" objects.
 */
int irr_database_clean (irr_database_t *database) {
  char dbfilename[MAXPATHLEN], cleanfilename[MAXPATHLEN];
  char buffer[BUFSIZE];
  char newdb[256];
  FILE *clean_fp, *db_fp;
  irr_database_t *cleaned_db;
  int line = 0, blank_line = 1;
  int long_line = 0;
  int i, skip = 0, clean_flag = 0;

  /* database cleaning disabled */
  if (database->no_dbclean) {
    trace (TR_ERROR, default_trace, "Clean aborted -- configured for %s as disabled!\n",
	   database->name);
    return (0);
  }

  sprintf (dbfilename, "%s/%s.db", IRR.database_dir, database->name);
  if ((db_fp = fopen (dbfilename, "r")) == NULL) {
    trace (TR_ERROR, default_trace, "Could not open db file %s:%s\n", 
	   dbfilename, strerror (errno));
    return (-1);
  }

  irr_clean_lock(database);	/* clean_lock still allows queries of the db */

  sprintf (cleanfilename, "%s/.%s.clean.db", IRR.database_dir, database->name);
  if ((clean_fp = fopen (cleanfilename, "w+")) == NULL) {
    trace (TR_ERROR, default_trace, "Could not open temporary file %s:%s\n", 
	   cleanfilename, strerror (errno));
    irr_clean_unlock (database);
    return (-1);
  }

  trace (NORM, default_trace, "Starting clean of %s\n", database->name);

  while (fgets (buffer, BUFSIZE, db_fp) != NULL) {
    line++;

    i = strlen(buffer);

    if (!long_line) {
      if ((i < 2)) {
        blank_line++;
      } else {
        if (blank_line > 0) {
	  if (skip != 0)
            skip = 0;
          if ( !strncasecmp ("*xx", buffer, 3) )
            skip = 1;
          blank_line = 0;
        }
      }

#ifdef NOTDEF
      /* this code has problems and can cause db corruption - disable for now */
      /* filter out comments if not at beginning of the database */
      /* should we really do this? */
      if ( (*buffer == '#') && (line > 42) ) {
        skip = 1;
        blank_line = 1; /* need to set in case object follows */
      }
#endif
    }
    long_line = (buffer[i-1] != '\n');

    if ((skip != 1) && (blank_line < 2))
      fwrite (buffer, 1, i, clean_fp);
    else {
      clean_flag = 1;	/* mark that database has been modified */
      /* trace (TR_TRACE, default_trace, "Skipping %s", buffer);*/ 
    }
  }

  fclose (db_fp);

  if (!clean_flag) {  /* if no changes to db file, we are done */
    fclose(clean_fp);
    unlink(cleanfilename);	/* clean up after ourselves */
    irr_clean_unlock (database); /* remove the clean lock */
    trace (NORM, default_trace, "Finished clean of %s; no changes\n", database->name);
    return (1);
  }

  sprintf (newdb, ".%s.clean", database->name);
  cleaned_db = new_database(newdb);
  cleaned_db->db_fp = clean_fp;
  cleaned_db->journal_fd = database->journal_fd;
  cleaned_db->obj_filter = database->obj_filter;

  scan_irr_file (cleaned_db, NULL, 0, NULL);

  irr_lock(database);

  radix_flush(database->radix_v4);
  radix_flush(database->radix_v6);
  database->radix_v4 = cleaned_db->radix_v4;
  database->radix_v6 = cleaned_db->radix_v6;
  if (database->hash)
    g_hash_table_destroy(database->hash);
  database->hash = cleaned_db->hash;
  if (database->hash_spec)
    g_hash_table_destroy(database->hash_spec);
  database->hash_spec = cleaned_db->hash_spec;

  /* clean up temporary database */
  irrd_free(cleaned_db->name);
  pthread_mutex_destroy (&cleaned_db->mutex_lock);
  pthread_mutex_destroy (&cleaned_db->mutex_clean_lock);
  irrd_free(cleaned_db);

  fclose (clean_fp);
  fclose (database->db_fp);	/* close old database file */

  /* move .<database>.clean.db to <database>.db */
  if (rename (cleanfilename, dbfilename) < 0) {
    trace (TR_ERROR, default_trace, "Could not rename file to %s:%s\n", 
	   dbfilename, strerror (errno));
    irr_update_unlock (database);
    return (-1);
  }

  if ((db_fp = fopen (dbfilename, "r+")) == NULL) {
    trace (TR_ERROR, default_trace, "Could not open db file %s:%s\n", 
	   dbfilename, strerror (errno));
    irr_update_unlock (database);
    return (-1);
  }

  database->db_fp = db_fp;
  irr_update_unlock (database);
  trace (NORM, default_trace, "Finished clean of %s\n", database->name);

  return (1);
}

void irr_export_timer (mtimer_t *timer, irr_database_t *db) {
  irr_database_export (db);
}

void irr_clean_timer (mtimer_t *timer, irr_database_t *db) {
  irr_database_clean (db);
}

int irr_database_export (irr_database_t *database) {
  char dbfile[BUFSIZE], tmp_export[BUFSIZE], tmp_export_gz[BUFSIZE];
  char export_name[BUFSIZE], command[BUFSIZE];
  u_long serial;
  int result;

  if (IRR.ftp_dir == NULL) {
    trace (TR_ERROR, default_trace, "Aborting export -- ftp directory not configured!\n");
    return (0);
  }

  /* new database maybe? */
  if (database->db_fp == NULL) {
    trace (TR_ERROR, default_trace, "Export aborted -- NULL file pointer for %s\n", database->name);
    return (-1);
  }

  sprintf (dbfile, "%s/%s.db", IRR.database_dir, database->name);

  if (IRR.tmp_dir != NULL) 
    sprintf (tmp_export, "%s/.%s.db.export", IRR.tmp_dir, database->name);
  else
    sprintf (tmp_export, "%s/.%s.db.export", IRR.ftp_dir, database->name);

  /* copy to export area (or maybe tmp directory) _atomically_ */
  irr_clean_lock (database); /* clean lock still allows reads of database */
  serial = database->serial_number; /* save the current serial number */
  if (irr_copy_file (dbfile, tmp_export, 1) != 1) {
    irr_clean_unlock (database);
    trace (TR_ERROR, default_trace, "Export failed! Aborting.\n");
    return (-1);
  }
  irr_clean_unlock (database);

  /* Check if we have configured a script to do the compression, i.e.,
    in order to hide the CRYPT-PW passwords before compressing.
    If nothing configured, fall back to using gzip  */
  sprintf (tmp_export_gz, "%s/.%s.db.export.gz", IRR.ftp_dir, database->name);
  if (database->compress_script != NULL)
    sprintf (command, "%s %s > %s", database->compress_script, tmp_export, tmp_export_gz); 
  else
    sprintf (command, "%s -q -c %s > %s", GZIP_CMD, tmp_export, tmp_export_gz); 
  trace (NORM, default_trace, "Running %s\n", command);
  result = system(command);
  remove(tmp_export);	/* clean-up after ourselves */
  if (result < 0) {
    remove(tmp_export_gz);
    trace (NORM, default_trace, "Error occured during compression!\n");
    return (-1);
  }

  /* move .<database>.db.export.gz to <database>.db.gz */
  if (database->export_filename != NULL) 
    sprintf (export_name, "%s/%s.db.gz", IRR.ftp_dir, database->export_filename);
  else
    sprintf (export_name, "%s/%s.db.gz", IRR.ftp_dir, database->name);
  trace (NORM, default_trace, "move %s -> %s\n", tmp_export_gz, export_name);
  if (rename (tmp_export_gz, export_name) < 0) {
    trace (TR_ERROR, default_trace, "Could not rename file to %s:%s\n", 
	   export_name, strerror (errno));
    return (-1);
  }

  write_irr_serial_export (serial, database);  /* update serial number in FTP directory */
  trace (NORM, default_trace, "Database %s copied to export directory\n",
	 database->name);

  return (1);
}

/*
 * irr_copy_file
 * Copies two files.  If add_eof_flag != 0, add # EOF to end of file
 */

int irr_copy_file (char *infile, char *outfile, int add_eof_flag) {
  char buf[MIRROR_BUFFER];
  FILE *fp1, *fp2;
  int n1 = 0, n2 = 0;

  trace (NORM, default_trace, "Starting copy of %s to %s\n", infile, outfile);

  if ((fp1 = fopen (infile, "r")) == NULL) {
    trace (TR_ERROR, default_trace, "*ERROR* Could not open %s for reading: "
	   " %s\n", infile, strerror (errno));
    return (-1);
  }

  if ((fp2 = fopen (outfile, "w")) == NULL) {
    trace (TR_ERROR, default_trace, "*ERROR* Could not open %s for writing: "
	   " %s\n", outfile, strerror (errno));
    fclose (fp1);
    return (-1);
  }

  while ((n1 = fread (buf, 1, MIRROR_BUFFER, fp1))) {
    if (n1 < 0) {
      trace (TR_ERROR, default_trace, 
	     "Encountered error copying (read failed) %s->%s (%d, %d): %s\n", 
	     infile, outfile, n1, n2, strerror (errno));
      fclose (fp1);
      fclose (fp2);
      return (-1);
    }

    if ((n2 = fwrite (buf, 1, n1, fp2)) != n1) {
      trace (TR_ERROR, default_trace, 
	     "Encountered error copying %s->%s (%d, %d): %s\n", 
	     infile, outfile, n1, n2, strerror (errno));
      fclose (fp1);
      fclose (fp2);
      return (-1);
    }
  }

  /* check feof and make sure no errors */
  if (ferror (fp1)) {
    trace (TR_ERROR, default_trace, "Encountered error copying %s: %s\n", 
	   infile, strerror (errno));
    fclose (fp1);
    fclose (fp2);
    return (-1);
  }

  /* write #EOF tag for nice export */
  if (add_eof_flag) {
    char buffer[100];
    sprintf (buffer, "\n# EOF\n\n"); 
    fwrite (buffer, 1, strlen (buffer), fp2); 
  }

  fclose (fp1);
  fclose (fp2);
  return (1);
}
  
/*
 * irr_check_serial_vs_journal:
 * For mirrored or authoritative databases will return 0 if the last
 * journal entry is out of sync with the CURRENTSERIAL.  A trace
 * message is logged.
 * Otherwise, 1 is returned.
 */

int irr_check_serial_vs_journal (irr_database_t *database) {
  uint32_t last_journal_sn = 0;

  /* If we are authoritative, or mirror, then check the last number
     of the journal files and verify that our CURRENTSERIAL is <= to it. */
  if (((database->flags & IRR_AUTHORITATIVE) == IRR_AUTHORITATIVE) ||
      (database->mirror_prefix != NULL)) {
    if (find_last_serial (database->name, JOURNAL_NEW, &last_journal_sn) == 0)
      last_journal_sn = 0;
  }

  if ((last_journal_sn > 0) &&
      (last_journal_sn > database->serial_number)) {
    trace (NORM, default_trace, 
           "WARNING: DB %s journal SN (%lu) > CURRENTSERIAL (%lu)!\n",
           database->name, last_journal_sn, database->serial_number);
    return (0);
  }

return (1);
}

int reopen_DB (irr_database_t *db, char *dir) {
  char tname[BUFSIZE+1];

  /* reopen the DB */
  sprintf (tname, "%s/%s.db", dir, db->name);
  return ((db->db_fp = fopen (tname, "r+")) != NULL);
}

int reopen_JOURNAL (irr_database_t *db, char *name) {

  return ((db->journal_fd = open (name, O_RDWR|O_APPEND, 0664)) >= 0);
}


/* Control the process of atomically replacing the *.DB files from
 * (tmp_dir) with the corresponding DB cache area files.  This function
 * assumes the files in (tmp_dir) are named identically with the
 * DB cache area objects they are intended to replace.
 *
 * Return:
 *  -1 if the operation took place without error
 *  -0 otherwise
 */
int replace_cache_db (irr_database_t *db, uii_connection_t *uii, char *tmp_dir) {
  int db_open, journal_open;
  char fname[BUFSIZE+1], uc[BUFSIZE];
  atom_finfo_t afinfo = {0};

  /* let the user know what's going on */
  if (uii != NULL) 
    uii_send_data (uii, "DB fetched.  Performing atomic replace...\r\n");

  /* we are going to rename the DB so it must be closed */
  db_open = (db->db_fp != NULL);
  fclose (db->db_fp);
  db->db_fp = NULL;

  /* this is the name of the DB we are importing */
  sprintf (fname, "%s.db", db->name);
  afinfo.tmp_dir = tmp_dir;
  
  /* atomically move the DB from the (tmp_dir) to the cache area */
  if (!atomic_move (IRR.database_dir, fname, uii, &afinfo)) {
    /* the atomic move failed, reopen the DB to restore */
    if (db_open &&
	!reopen_DB (db, IRR.database_dir)) {
      trace (ERROR, default_trace, 
	     "replace_cache_db (): could reopen (%s). exit (0)\n", db->name);
      exit (0);
    }

    return 0;
  }

  /* if the DB is mirrored then we need to move the *.CURRENTSERIAL file 
   * to the cache area */
  if (db->mirror_prefix != NULL) {
    strcpy (uc, db->name);
    convert_toupper (uc);
    sprintf (fname, "%s.CURRENTSERIAL", uc);
    if (!atomic_move (IRR.database_dir, fname, uii, &afinfo)) {
      if (db_open &&
	  !reopen_DB (db, IRR.database_dir)) {
	trace (ERROR, default_trace, 
	       "replace_cache_db (): could reopen (%s). exit (0)\n", db->name);
	exit (0);
      }
      return 0;
    }
  }
  
  /* JOURNAL file processing.  the old JOURNAL files need to be
   * removed since we are importing a new DB */

  /* close the JOURNAL file */
  journal_open = 0;
  if (db->journal_fd >= 0) {
    close (db->journal_fd);
    journal_open = 1;
  }

  db->journal_fd = -1;

  /* atomically remove the old JOURNAL files */
  sprintf (fname, "%s.%s", db->name, SJOURNAL_NEW);
  sprintf (uc,    "%s.%s", db->name, SJOURNAL_OLD);
  if (!atomic_del (IRR.database_dir, fname, uii, &afinfo) ||
      !atomic_del (IRR.database_dir, uc,    uii, &afinfo)) {
    if ((db_open && !reopen_DB      (db, IRR.database_dir)) ||
	(journal_open && !reopen_JOURNAL (db, uc))) {
      trace (ERROR, default_trace, 
	     "replace_cache_db (): could reopen (%s). exit (0)\n", db->name);
      exit (0);
    }
    return 0;
  }

  /* clean up the *.bak files */
  atomic_cleanup (&afinfo);

  return 1;
}

/* Control process of fetching (db->name) from a remote ftp site.
 *
 * If (ftp_url) is non-null then it will be used instead of 
 * (db->remote_ftp_url) for the remote ftp URL.  If (db->name)
 * is successfully fetched it is loaded into the DB cache and
 * new indexes are rebuilt.  irrdcacher and wget are the externals
 * that actually perform the fetch operation.
 *
 * Input:
 *  -pointer to a database info struct (db)
 *  -ftp URL to override the (db->remote_ftp_url) value (ftp_url)
 *
 * Return:
 *  -1 if the (db->name) was sucessfully fetched
 *  -0 otherwise
 */
int fetch_remote_db (irr_database_t *db, char *ftp_url) {
  int fetched = 0;
  char *p;

  /* save db->remote_ftp_url and override its value
   * if (ftp_url) is non-NULL */
  p = db->remote_ftp_url;
  if (ftp_url != NULL)
    db->remote_ftp_url = ftp_url;

  /* try fetching (db->name) from (db->remote_ftp_url) */
  if (db->remote_ftp_url != NULL) {
    
    irr_update_unlock (db);
    if ((fetched = uii_irr_irrdcacher (NULL, strdup (db->name))))
      trace (NORM, default_trace, "Remote fetch successful (%s)\n",
	     db->name);
    else
      trace (NORM, default_trace, "Remote fetch unsuccessful (%s)\n",
	     db->name);
    irr_update_lock (db);
  }

  /* restore original value */
  db->remote_ftp_url = p;

  return fetched;
}

/* Manage the process of fetching a DB from a remote ftp site.
 *
 * Function decides what remote sites to attempt and in what order.
 * Function will not attempt to fetch authoritative DB's.
 *
 * Input:
 *  -pointer to a database info struct (db)
 *
 * Return:
 *  -1 if the (db->name) was sucessfully fetched
 *  -0 otherwise
 */
int ftp_db_fetch (irr_database_t *db) {
  int db_fetched = 0;
  
  /* sanity check */
  if (db->flags & IRR_AUTHORITATIVE)
    return 0;

  /* Choice #1: fetch from the user specified db->remote_ftp_url */
  if (db->remote_ftp_url != NULL) {
    trace (NORM, default_trace, "Trying remote DB fetch (%s)\n",
	   db->remote_ftp_url);
    db_fetched = fetch_remote_db (db, NULL);
  }
  
  /* JW: later try getting the info from repository objects */
  
  /* Choice #2: fetch from ftp.radb.net */
  if (!db_fetched) {
    trace (NORM, default_trace, "Trying remote DB fetch (%s)\n",
	   DEF_FTP_URL);
    db_fetched = fetch_remote_db (db, DEF_FTP_URL);
  }
  
  if (!db_fetched)
    trace (NORM, default_trace, "Abandon remote fetch effort for (%s)\n",
	   db->name);

  return db_fetched;
}
