/* 
 * $Id: dbm_support.c,v 1.3 2001/07/13 19:15:40 ljb Exp $
 * originally Id: dbm_support.c,v 1.11 1998/06/25 19:47:56 gerald Exp 
 */

#include "config.h"

#ifdef USE_GDBM

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "mrt.h"
#include "trace.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <signal.h>
#include "config_file.h"
#include <fcntl.h>
#include "irrd.h"

extern trace_t *default_trace;

#include <gdbm.h>
 
/* irr_initialize_dbm_file
 * It does not exist on disk, we need to create it
 */
GDBM_FILE irr_initialize_dbm_file (char *name) {
  char file[BUFSIZE];
  int value = 3000;
  GDBM_FILE db;

  sprintf (file, "%s/%s.dbm", IRR.database_dir, name);

  /* if it's there, get rid of it.  *.dbm could be corrupt or
   * otherwise tainted.
   */
  unlink (file);
  if ((db = gdbm_open(file, 512, GDBM_NEWDB | GDBM_FAST, 0666, 0)) == NULL) {
    trace (ERROR, default_trace, "Fatal error, could not open dbm file for %s\n", name);
    return (NULL);
  }
  
  gdbm_setopt(db, GDBM_CACHESIZE, &value, sizeof(int));

  trace (NORM, default_trace, "Initialized DBM (GDB) file for %s\n", name);
  return (db);
}


/* irr_dbm_delete
 * Delete a dbm entry
 */
int irr_dbm_delete (GDBM_FILE db, char *key, char *name) {
  datum keydatum;

  keydatum.dptr = key;
 keydatum.dsize = strlen (key);

  if (gdbm_delete (db, keydatum) < 0) {
    trace (ERROR, default_trace, "DBM delete failed for  %s\n", name);
    return (-1);
  }

  return (1);
}


hash_item_t *irr_dbm_fetch (GDBM_FILE db, char *key) {
  datum keydatum, answer;
  hash_item_t *hash_item;

  keydatum.dptr = key;
  keydatum.dsize = strlen (key);
  
  answer = gdbm_fetch(db, keydatum);
  
  if (answer.dptr != NULL) {
    hash_item = New (hash_item_t);
    hash_item->value = malloc (answer.dsize);
    memcpy (hash_item->value, answer.dptr, answer.dsize);
    Delete (answer.dptr);
    return (hash_item);
  }

  return (NULL);
}




/* irr_dbm_store
 */
int irr_dbm_store (GDBM_FILE db, u_char *key, u_char *value, int len, 
                   char *name) {
  datum key_datum, value_datum;

  key_datum.dptr = key;
  key_datum.dsize = strlen (key);
  value_datum.dptr = value;
  value_datum.dsize = len;

  if (gdbm_store(db, key_datum, value_datum, GDBM_REPLACE) < 0) {
    trace (ERROR, default_trace, "DBM store failed for  %s\n", name);
    return (-1);
  }

  return (1);
}


/*
 * irr_open_dbm_file
 * In the new IRRd way of doing things (coming soon),
 * we try to open dbm file on disk first. If it exists,
 * we don't bother building indicies if we are in use_disk mode.
 * If we are using memory, we -- of course -- need to build indicies
 * in memory.
 */ 
int irr_open_dbm_file (irr_database_t *database) {
  char file[BUFSIZE], file_spec[BUFSIZE], *key;
  hash_spec_t *hspec = NULL;
  irr_hash_string_t *hash_string = NULL;
  /*int value = 1000;*/
  GDBM_FILE db, db_spec;
  int time;
  struct stat sbuf;
  FILE *fd;
  
  sprintf (file, "%s/%s.dbm", IRR.database_dir, database->name);

  if ((db = gdbm_open(file, 512, GDBM_READER | GDBM_FAST | GDBM_WRITER, 0666, 0)) 
      == NULL) {
    trace (NORM, default_trace, 
	   "Could not open dbm file for %s. Assuming we are running for first time\n", 
	   database->name);
    return (0);
  }
  trace (NORM, default_trace, "Opened existing DBM (GDB) file for %s\n", 
	 database->name);

  
  sprintf (file_spec, "%s/%s_spec.dbm",  IRR.database_dir, database->name);
  if ((db_spec = 
       gdbm_open(file_spec, 512, 
		 GDBM_READER | GDBM_FAST | GDBM_WRITER, 0666, 0)) == NULL) {
    trace (ERROR, default_trace, 
	   "Could not open spec dbm file for %s\n", database->name);
    return (0);
  }


  /* open database.db ASCII file */
  sprintf (file, "%s/%s.db", IRR.database_dir, database->name);
  if ((fd = fopen (file, "r+")) == NULL) {
    trace (NORM, default_trace, "**** ERROR **** Could not open %s (%s)!\n", 
	   file, strerror (errno));
    return (0);
  }
  fseek (fd, 0, SEEK_SET);

  database->fd = fd;
  database->dbm = db;
  database->dbm_spec = db_spec;

  /* need to fill in other statistics, like # autnum, num route, etc */
  /* JMH - This call looks unnecessary.  The caller of this function does
     this later and nothing following needs this data. */
  scan_irr_serial (database);

  /* see database.c for the format of the hspec data */ 
  key = strdup ("@STATS");
  hspec = fetch_hash_spec (database, key, UNPACK);

  if (hspec == NULL) {
    trace (NORM, default_trace, "**** ERROR **** DBM file may be corrupted...\n");
    return (0);
  }

  hash_string = LL_GetHead (hspec->ll_1);
  sscanf (hash_string->string, "%d#%d#%d#%ld#%ld", &time, 
	  (int *) &database->db_syntax,
	  &database->bytes,
	  (long *) &database->num_objects[ROUTE],
	  (long *) &database->num_objects[AUT_NUM]);
  Delete (key);
  /* -- end of loading special key */

  /* set master syntax type. We should really check all database
   * agree...
   * JW: not necessary.  We now allow mixed db syntax types.
   */
  IRR.database_syntax = database->db_syntax;

  /* get the *.dbm last modification time as the 'last loaded' time */
  sprintf (file, "%s/%s.dbm", IRR.database_dir, database->name);
  if ((fd = fopen (file, "r")) != NULL) {
    char timebuf[32];
    fstat(fileno (fd), &sbuf);
    database->time_loaded = sbuf.st_mtime;
#ifdef HAVE_LIBPTHREAD
    ctime_r ( &database->time_loaded, timebuf);
#else
    strcpy (timebuf, ctime (&database->time_loaded));
#endif /* HAVE_LIBPTHREAD */
    timebuf[strlen (timebuf) -1] = '\0';
    trace (NORM, default_trace, "setting db creation time (%s)...\n", timebuf);
    fclose (fd);
  }
  else
    trace (NORM, default_trace, "could not set db creation time :(\n");

  return (1);
}


/* turn on fast indexing. This is fast, but dangerous if IRRd crashes... */
int dbm_fast (irr_database_t *database) {
  int true = 1;

  if (IRR.use_disk) {
    gdbm_setopt(database->dbm, GDBM_FASTMODE, &true, sizeof(int));
    gdbm_setopt(database->dbm_spec, GDBM_FASTMODE, &true, sizeof(int));
  }
  return (1);
}


/* turn off fast indexing. We now ask GDBM to commit everything to disk */
int dbm_slow (irr_database_t *database) {
  int false = 0;
  if (IRR.use_disk) {
    gdbm_setopt(database->dbm, GDBM_FASTMODE, &false, sizeof(int));
    gdbm_setopt(database->dbm_spec, GDBM_FASTMODE, &false, sizeof(int));
  }
  return (1);
}



#endif /* USE_GDBM */
