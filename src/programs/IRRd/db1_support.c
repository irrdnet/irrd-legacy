/* 
* $Id: db1_support.c,v 1.2 2001/07/13 19:15:39 ljb Exp $
 * originally Id: db1_support.c,v 1.5 1998/06/25 19:47:56 gerald Exp 
*/

#include "config.h"

#ifdef USE_DB1

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

#include <sys/stat.h>
#include <sys/types.h>
#include <limits.h>
#include <db.h>

/* irr_initialize_db1_file
 */
DB *irr_initialize_dbm_file (char *name) {
  char file[BUFSIZE];
  DB *db;

  sprintf (file, "%s/%s.db1", IRR.database_dir, name);
printf ("inside irr_initialize_dbm_file () file name (%s)\n", file);
  (void)unlink(file);
  if ((db = dbopen (file, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR, 
	            DB_HASH, NULL)) == NULL) {
    trace (ERROR, default_trace, "Could not open db1 file for (%s) (%s)\n", 
           name, strerror (errno));
    return (NULL);
  }

printf ("irr_initialize_dbm_file () returns intialized dbm file OK! (%s)\n", file);
    trace (NORM, default_trace, "Initialized DBM (DB1) file for %s\n", name);
    return (db);
}


int irr_dbm_delete (DB *db, char *key, char *name) {
  DBT keydatum;

  keydatum.data = key;
  keydatum.size = strlen (key);

  switch (db->del(db, &keydatum, 0)) {
  case 0: return (1);
  case 1: 
    trace (NORM, default_trace, "WARN: DBM delete, no such key for %s\n", name);
    return (-1);
  case -1:
    trace (NORM, default_trace, "ERROR: DBM delete failed (%s) (%s)\n", name, strerror (errno));
    return (-1);
  }

  /* NOTREACHED */
  trace (ERROR, default_trace, "ERROR: DBM delete should never get here\n");
  return (-1);
}

hash_item_t *irr_dbm_fetch (DB *db, char *key) {
  DBT keydatum, answer;
  hash_item_t *hash_item;

  keydatum.data = key;
  keydatum.size = strlen (key);
  
  switch (db->get (db, &keydatum, &answer, 0)) {
  case 0: 
    hash_item = New (hash_item_t);
/*
    hash_item->value = answer.data;
*/
    hash_item->value = malloc (answer.size);
    memcpy (hash_item->value, answer.data, answer.size);
/*
    Delete (answer.data);
*/
    return (hash_item);
  case 1: return (NULL);
  case -1:
    trace (NORM, default_trace, "ERROR: DBM fetch failed (%s)\n", strerror (errno));
    return (NULL);
  }

  /* NOTREACHED */
  trace (ERROR, default_trace, "ERROR: DBM fetch should never get here\n");
  return (NULL);
}


/* irr_dbm_store
 */
int irr_dbm_store (DB *db, u_char *key, u_char *value, int len, 
                   char *name) {

  DBT key_datum, value_datum;

  key_datum.data = key;
  key_datum.size = strlen (key);
  value_datum.data = value;
  value_datum.size = len;

  switch (db->put (db, &key_datum, &value_datum, 0)) {
  case 0: return (1);
  case -1:
    trace (ERROR, default_trace, "ERROR: DBM store failed (%s) (%s)\n", name, strerror (errno));
    return (-1);
  }

  /* NOTREACHED */
  trace (ERROR, default_trace, "ERROR: DBM store should never get here\n");
  return (-1);
}


#endif /* HAVE_DB1_H */
