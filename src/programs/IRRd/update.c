/*
 * $Id: update.c,v 1.4 2002/02/04 20:53:57 ljb Exp $
 * originally Id: update.c,v 1.46 1998/07/29 21:15:18 gerald Exp 
 */

#include <stdio.h>
#include <string.h>
#include "mrt.h"
#include "trace.h"
#include <time.h>
#include <signal.h>
#include "config_file.h"
#include <fcntl.h>
#include "irrd.h"
#include "irrd_prototypes.h"

extern trace_t *default_trace;

static int build_secondary_keys (irr_database_t *db, irr_object_t *object);

void mark_deleted_irr_object (irr_database_t *database, u_long offset) {
  char *xx = "*xx";

  if (fseek (database->db_fp, offset, SEEK_SET) < 0) {
    perror ("seek error");
    trace (NORM, default_trace, "** Error ** fseek failed in mark_deleted");
  }
  if (fwrite (xx, 1, 3, database->db_fp) == 0) 
    perror ("frwite failed");

  fflush (database->db_fp);
}


/* make the hash entries for the !gas command */
void add_gas_answer (irr_database_t *db, irr_object_t *object, 
                     enum SPEC_KEYS id) {

  char str_origin[BUFSIZE], gas_key[BUFSIZE];

  if (object->withdrawn && object->mode != IRR_DELETE)
    return;

  sprintf (str_origin, "%d", object->origin);
  make_gas_key (gas_key, str_origin);

  if (object->mode == IRR_DELETE)
    memory_hash_spec_remove (db, gas_key, id, object->name);
  else
    memory_hash_spec_store (db, gas_key, id, NULL, NULL, object->name);
}

/* make the hash entries for the !h community (route) list command */
void add_comm_list_answer (irr_database_t *db, char *prefix, 
                           LINKED_LIST *ll_comm, int withdrawn,
			   int mode, enum SPEC_KEYS id) {
  char *community, comm_key[BUFSIZE];

  if (ll_comm == NULL ||
      (withdrawn && mode != IRR_DELETE))
    return;

  LL_ContIterate (ll_comm, community) {
    make_comm_key (comm_key, community);
    if (mode == IRR_DELETE)
      memory_hash_spec_remove (db, comm_key, id, prefix);
    else
      memory_hash_spec_store (db, comm_key, id, NULL, NULL, prefix);
  }

}

void add_spec_keys (irr_database_t *db, char *key, irr_object_t *object,
		    enum SPEC_KEYS id) {
  char *maint, *as_set, new_key[BUFSIZE];

  if (object->ll_mbr_of == NULL)
    return;

  /* this key is for (maint|as-set) hash lookups from set objects */

  if (object->ll_mnt_by != NULL) {
    LL_ContIterate (object->ll_mnt_by, maint) {
      LL_ContIterate (object->ll_mbr_of, as_set) {
        make_spec_key (new_key, maint, as_set);
        if (object->mode == IRR_DELETE)
          memory_hash_spec_remove (db, new_key, id, key);
        else
          memory_hash_spec_store (db, new_key, id, NULL, NULL, key);
      }
    }
  }

  /* add this key for fast ANY lookups */

  LL_ContIterate (object->ll_mbr_of, as_set) {
    make_spec_key (new_key, NULL, as_set);
    if (object->mode == IRR_DELETE)
      memory_hash_spec_remove (db, new_key, id, key);
    else
      memory_hash_spec_store (db, new_key, id, NULL, NULL, key);
  }
}

/* load_irr_object
 * We need to find an object before we can delete it 
 * Basically, we need to fill in the object structure (with all of the old keys)
 */
irr_object_t *load_irr_object (irr_database_t *database, irr_object_t *irr_object) {
  irr_object_t *old_irr_object = NULL;
  int ret;
  u_long offset, len;

  /* route objects are special because we need both the key AND the autnum */
  if (irr_object->type == ROUTE) 
    ret = seek_route_object (database, irr_object->name, irr_object->origin,
                             &offset, &len, 1);
  else /* fill in object->offset and object->len */
    ret = find_object_offset_len (database, irr_object->name, 
                                  irr_object->type, &offset, &len);

  /* object doesn't really exist -- nothing to delete */
  if (ret < 1)
    return (NULL);

  /* JW: I'm wondering about locking?  Should we lock? */
  if (fseek(database->db_fp, offset, SEEK_SET) < 0)
    trace (NORM, default_trace, "** Error ** fseek failed in load_irr_obj");
  old_irr_object = (irr_object_t *) scan_irr_file_main (database->db_fp, database, 
							0, SCAN_OBJECT);
  if (old_irr_object)
    old_irr_object->offset = offset;

  return (old_irr_object);
}

/* irr_special_indexing_store
 * store "special" keys, or keys that we cannot just dump into the
 * the generic hash table. These include route objects (which need specia
 * radix magic) and AD macros (which need??)
 *
 * return 1 is we still need to store this object in the hash
 */
int irr_special_indexing_store (irr_database_t *database, 
                                irr_object_t *irr_object) {
  char *key, spec_key[BUFSIZE];
  int store_hash = 1;

  switch (irr_object->type) {
  case ROUTE:
    add_gas_answer (database, irr_object, GASX);
    add_comm_list_answer (database, irr_object->name, 
                          irr_object->ll_community, irr_object->withdrawn,
			  irr_object->mode, COMM_LISTX);
    add_spec_keys (database, irr_object->name, irr_object, SET_MBRSX);
    if (irr_object->mode == IRR_DELETE)
      delete_irr_route (database, irr_object->name, irr_object);
    else
      add_irr_route (database, irr_object->name, irr_object);
    store_hash = 0;
    break;
  case AUT_NUM:
    add_spec_keys (database, irr_object->name, irr_object, SET_MBRSX);
    break;
  case IPV6_SITE:
    LL_Iterate (irr_object->ll_prefix, key) {
      if (irr_object->mode == IRR_DELETE)
        delete_irr_route (database, key, irr_object);
     else
       add_irr_route (database, key, irr_object); 
    }
    store_hash = 0;
    break;
  case AS_MACRO: /* RIPE181 only */
    make_setobj_key (spec_key, irr_object->name);
    if (irr_object->mode == IRR_DELETE)
      memory_hash_spec_remove (database, spec_key, AS_MACROX, NULL);
    else
      memory_hash_spec_store (database, spec_key, AS_MACROX, 
                              irr_object->ll_as, NULL, NULL);
    break;
  case AS_SET: case RS_SET: /* RPSL only */
    make_setobj_key (spec_key, irr_object->name);
    if (irr_object->mode == IRR_DELETE)
      memory_hash_spec_remove (database, spec_key, SET_OBJX, NULL);
    else
      memory_hash_spec_store (database, spec_key, SET_OBJX, 
                              irr_object->ll_as, irr_object->ll_mbr_by_ref, 
                              NULL);
    break;
  default:
    store_hash = 1;
    break;
  }

  return (store_hash);
}

void back_out_secondaries (irr_database_t *db, irr_object_t *object, char *buf) {
  char *p, *q;

  for (q = buf; (p = strchr (q, ' ')) != NULL; q = p) {
    *p++ = '\0';
    irr_database_remove (db, q, SECONDARY, object->type, 
			 object->offset, object->len);
  }
}

/* JW It looks like the irr_database_store () routine allows 
 * duplicate PRIMARY keys.  Need to fix so it returns 
 * an error
 */
/* Given a PERSON object, make its secondary keys and it's complete
 * 'person: nic-hdl:' key (which is a primary key).  So this routine
 * inappropriately named.
 *
 * Return:
 *   void
 */
int build_secondary_keys (irr_database_t *db, irr_object_t *object) {
  char buf[256], buf1[256], *cp, *last, *p;
  int n = 0, ret_code = 1;
  int (*func)(irr_database_t *, char *, u_short, 
	      enum IRR_OBJECTS, u_long,  u_long);

  switch (object->type) {
  case PERSON:
    if (object->mode == IRR_DELETE)
      func = &irr_database_remove;
    else
      func = &irr_database_store;

    p = buf;
    strcpy (buf1, object->name);
    cp = buf1;
    strtok_r (cp, " ", &last);
    while (cp != NULL) {
      whitespace_remove (cp);

      if ((ret_code = (*func) (db, cp, SECONDARY, object->type, 
			       object->offset, object->len)) < 0) {
	/* an error occured, remove any secondary key's we have added */
	if (object->mode != IRR_DELETE) {
	  if (p != buf) {
	    *p++ = ' ';
	    *p = '\0';
	    back_out_secondaries (db, object, buf);
	  }
	  break;
	}
      }
      n = strlen (cp);
      if (p != buf)
	*p++ = ' ';
      memcpy (p, cp, n);
      p += n;	
      cp = strtok_r (NULL, " ", &last);
    }
    if (object->nic_hdl) {
      n = strlen (object->nic_hdl);
      *p++ = ' ';
      memcpy (p, object->nic_hdl, n);
      *(p + n) = '\0';
      if ((ret_code = (*func) (db, buf, PRIMARY, object->type, 
			       object->offset, object->len)) < 0) {
	if (object->mode != IRR_DELETE) {
	  *p = '\0';
	  back_out_secondaries (db, object, buf);
	}
      }
      else if ((*func) (db, object->nic_hdl, SECONDARY, object->type, 
			object->offset, object->len) < 0 &&
	       object->mode != IRR_DELETE) {
	  ret_code = -1;
	  irr_database_remove (db, buf, PRIMARY, object->type,  
			       object->offset, object->len);
      }
    }
    break;
  default:
    break;
  }

  return ret_code;
}

/* scan_process_object
 * We have scanned an entire object from the db, update, journal, whatever file
 * Now process the darn thing
 */
int add_irr_object (irr_database_t *database, irr_object_t *irr_object) {
  long svoffset = 0, offset;
  int ret_code = 1, store_hash = 0;

  /* if update (e.g. mirror or user update), save to end of main database */
  /* append the new object to the end of the db file */
  if (irr_object->mode == IRR_UPDATE) {
    offset = copy_irr_object (irr_object->fp, (long) irr_object->offset, 
			      database, irr_object->len);
    if (offset < 0) {
      trace (ERROR, default_trace, 
	     "DB (%s) file error, could not add object (%s) to EOF\n",
	     database->name, irr_object->name);
      return -1;
    }
    svoffset = irr_object->offset;
    irr_object->offset = (u_long) offset;
  }

  /* deal with primary key; JW later need to change to detect error */
  store_hash = irr_special_indexing_store (database, irr_object); 
  
  if (store_hash) {
    if ((ret_code = irr_database_store (database, irr_object->name, PRIMARY, 
					irr_object->type, irr_object->offset, 
					irr_object->len)) > 0) {
      /* Routine will build the PERSON secondary and 'person: hic-hdl:' primary key */
      if (irr_object->type == PERSON &&
	  (ret_code = build_secondary_keys (database, irr_object)) < 0)
	irr_database_remove (database, irr_object->name, PRIMARY, 
			     irr_object->type, irr_object->offset, irr_object->len);
    }
  }

  if (irr_object->mode == IRR_UPDATE) {
    if (ret_code > 0)
      trace (NORM, default_trace, "Object %s added\n", irr_object->name);
    else
      trace (ERROR, default_trace, 
	     "ERROR: Disk or indexing error: key (%s)\n", irr_object->name);
  }

  /* statistics */
  if (ret_code > 0) {
    database->num_objects[irr_object->type]++;
    database->bytes += irr_object->len;
  }

  if (irr_object->mode == IRR_UPDATE) 
    irr_object->offset = svoffset;

  return ret_code;
}

/* delete_irr_object 
 * First, load the current version of the object. We need to check a) that it
 * exists, and b) we need to get the current offset/len and the secondary keys
 * Once we have all of this, delete the indicies, mark the database file on disk
 */
int delete_irr_object (irr_database_t *database, irr_object_t *irr_object,
                       u_long *db_offset) {
  int ret_code = 1, store_hash = 0;

  irr_object_t *stored_irr_object;

  if ((stored_irr_object = load_irr_object (database, irr_object)) == NULL) {
    if (irr_object->mode == IRR_DELETE)
      trace (NORM, default_trace, "Object %s not found -- delete failed\n",
	     irr_object->name);
    return -1;
  }

  /* remove the special indexes for this object */
  stored_irr_object->mode = IRR_DELETE;
  store_hash = irr_special_indexing_store (database, stored_irr_object); 

  if (store_hash) {
    if ((ret_code = irr_database_remove (database, irr_object->name, PRIMARY, 
					 stored_irr_object->type, stored_irr_object->offset, 
					 stored_irr_object->len)) > 0)
      if (stored_irr_object->type == PERSON &&
	  (ret_code = build_secondary_keys (database, stored_irr_object)) < 0)
	irr_database_store (database, irr_object->name, PRIMARY, 
			    stored_irr_object->type, stored_irr_object->offset, 
			    stored_irr_object->len);
  }
  
  /* statistics */
  if (ret_code > 0) {
    database->num_objects[irr_object->type]--;
    *db_offset = stored_irr_object->offset;
    trace (NORM, default_trace, "Object %s deleted!\n", irr_object->name);
  }

  /* release memory */
  Delete_IRR_Object (stored_irr_object);

  return ret_code;
}

