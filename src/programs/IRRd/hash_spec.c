/*
 * $Id: hash_spec.c,v 1.1.1.1 2000/02/29 22:28:30 labovit Exp $
 * originally Id: hash_spec.c,v 1.14 1998/07/20 01:22:03 labovit Exp 
 */

/* routines for handling indicies in hash or on disk dbm files */

#include <stdio.h>
#include <string.h>
#include "mrt.h"
#include "trace.h"
#include <time.h>
#include <signal.h>
#include "config_file.h"
#include <fcntl.h>
#include <ctype.h>
#include "irrd.h"

extern trace_t *default_trace;

int hash_spec_store_value (irr_database_t *database, char *key, hash_spec_t *value);

/* called when indexes are stored in memory */
int irr_spec_hash_store (irr_database_t *database, char *key, char *value) {
  hash_item_t *hash_item;

  hash_item = New (hash_item_t);
  hash_item->key = strdup (key);
  hash_item->value = value;
  HASH_Insert (database->hash_spec, hash_item);
  return (1);
}
  
/* make a string from a linked list, each item is seperated by a ' ' */
void util_put_ll (LINKED_LIST *ll, char **cp) {
  irr_hash_string_t *p;
  char *c = *cp;

  *c = '\0'; 

  LL_Iterate (ll, p) {
    *c++ = ' ';
    strcpy (c, p->string);
    while (*++c); 
  }

  /* make cp point to the next available char */
  *cp = ++c;
}

/* make a linked list from a string, items in the string are seperated by a ' ' */
void util_get_ll (LINKED_LIST **ll, u_long len, u_long items, char **cp) {
  char *a, *b;
  irr_hash_string_t tmp;

  a = b = *cp + 1;
  while (*a == ' ') {
    a++;
    b++;
  }

  /*(*ll) = LL_Create (LL_DestroyFunction, free, NULL);*/
  
  (*ll) = LL_Create (LL_Intrusive, True, 
		     LL_NextOffset, LL_Offset (&tmp, &tmp.next),
		     LL_PrevOffset, LL_Offset (&tmp, &tmp.prev),
		     LL_DestroyFunction, delete_irr_hash_string, 
		     0);

  while (items > 0) {
    if ((b = strchr (a, ' ')) != NULL)
      *b = '\0';
    LL_Add ((*ll), new_irr_hash_string (a));
    if (b)
      *b = ' ';
    a = ++b;
    items--;
  }
  /* make cp point to the begining of the next string */
  *cp += len; 
}

/* Marshal the hash_spec_t struct into a hash_item_t struct.
 * this means flattening out the linked lists (ie, put both
 * ll's into a single char string).
 */
void store_hash_spec (irr_database_t *database, hash_spec_t *hash_item) { 
  char *cp, *buf;
  u_short _id;
  u_int str_size;
  hash_spec_t *hash_x;

  /* compute the size of the packed value string */
  str_size = sizeof (_id);
  str_size += 4 * sizeof (str_size);

  /* the lengths should include the '\0' at the end */
  str_size += hash_item->len1 + hash_item->len2 + 2;

  /* now pack up the value part */
  cp = buf = malloc (str_size);
  _id = hash_item->id;
  UTIL_PUT_NETSHORT (_id, cp); 
  UTIL_PUT_NETLONG  (hash_item->len1, cp); 
  UTIL_PUT_NETLONG  (hash_item->items1, cp);
  UTIL_PUT_NETLONG  (hash_item->len2, cp); 
  UTIL_PUT_NETLONG  (hash_item->items2, cp);
  util_put_ll       (hash_item->ll_1, &cp);
  util_put_ll       (hash_item->ll_2, &cp);

#if (defined(USE_GDBM) || defined(USE_DB1))
  if (IRR.use_disk == 1) {
    irr_dbm_store (database->dbm_spec, hash_item->key, buf, 
                   str_size, database->name);
    Delete (buf);
  }
#endif /* USE_GDBM || USE_DB1 */
  if (IRR.use_disk == 0) {
    hash_x = HASH_Lookup (database->hash_spec, hash_item->key);
    if (hash_x != NULL) HASH_Remove (database->hash_spec, hash_x);
    irr_spec_hash_store (database, hash_item->key, buf);
  }
}

void remove_hash_spec (irr_database_t *db, char *key) {
  /* printf("enter remove_hash_spec( key-(%s))\n",key); */
#if (defined(USE_GDBM) || defined(USE_DB1))
  if (IRR.use_disk == 1) 
    irr_dbm_delete (db->dbm_spec, key, db->name);
#endif /* USE_GDBM || USE_DB1 */
  if (IRR.use_disk == 0)  {
    HASH_RemoveByKey (db->hash_spec, key);
}
}

hash_spec_t *fetch_hash_spec (irr_database_t *database, char *key, 
                              enum FETCH_T mode) {
  char *cp;
  u_short _id;
  hash_item_t *hash_item = NULL;
  hash_spec_t *hash_sval = NULL;

#if (defined(USE_GDBM) || defined(USE_DB1))
  if (IRR.use_disk == 1) {
    if (database->dbm_spec != NULL)
      hash_item = irr_dbm_fetch (database->dbm_spec, key);    
  }
  else
#endif /* USE_GDBM || USE_DB1 */
    hash_item = HASH_Lookup (database->hash_spec, key);

  if (hash_item) {
    cp = hash_item->value;
    hash_sval = New (hash_spec_t);
    hash_sval->key = strdup (key);
    UTIL_GET_NETSHORT (_id, cp);
    hash_sval->id = _id;
    UTIL_GET_NETLONG (hash_sval->len1, cp);
    UTIL_GET_NETLONG (hash_sval->items1, cp);
    UTIL_GET_NETLONG (hash_sval->len2, cp);
    UTIL_GET_NETLONG (hash_sval->items2, cp);
    if (mode == UNPACK) {
      util_get_ll (&hash_sval->ll_1, hash_sval->len1, 
                   hash_sval->items1, &cp);
      util_get_ll (&hash_sval->ll_2, hash_sval->len2, 
                   hash_sval->items2, &cp);
    }
    else  {/* FAST mode, just return the unpacked item, ie !gas */
      hash_sval->unpacked_value = hash_item->value;
      hash_sval->gas_answer = cp; /* points to the first string/gas answer */
    }
  }    
  else
    hash_sval = NULL;


#if (defined(USE_GDBM) || defined(USE_DB1))
  if (IRR.use_disk == 1 && hash_item != NULL) {
    if (mode == UNPACK)
      irr_hash_destroy (hash_item);
    else { /* calling routine will free the value part */
      Delete (hash_item->key);
      Delete (hash_item);
    }
  }
#endif /* USE_GDBM || USE_DB1 */

  return (hash_sval);
}


void memory_hash_spec_del (hash_spec_t *hash_value, enum SPEC_KEYS id, 
		           char *blob_item) {
  irr_hash_string_t *p;

  switch (id) {
    case AS_MACROX:
    case SET_OBJX:
      hash_value->len1 = hash_value->len2 = 0; 
      hash_value->items1 = hash_value->items2 = 0;
      LL_Clear (hash_value->ll_1);
      LL_Clear (hash_value->ll_2);
      break;
    case SET_MBRSX:
    case GASX:
    case COMM_LISTX:
      if (blob_item == NULL)
        return;
      LL_Iterate (hash_value->ll_1, p) {
        if (!strcmp (blob_item, p->string)) {
          LL_Remove (hash_value->ll_1, p);
          hash_value->items1--;
          hash_value->len1 -= strlen (blob_item) + 1; 
          break;
        }
      }
      break;
    case ROUTE_BITSTRING:   /* just match first $d# -- this is the origin */ 
      LL_Iterate (hash_value->ll_1, p) {
        if (!strncmp (blob_item, p->string, strlen (blob_item))) {
          hash_value->len1 -= strlen (p->string) + 1; 
	  LL_Remove (hash_value->ll_1, p);
          hash_value->items1--;
          break;
	}
      }
      break;
    default:
      trace (ERROR, default_trace, "memory_hash_spec_del() error.  Got an unknown index id-type, cannot perform delete operation id-(%d)\n!", id);
      break;
  };

}

int memory_hash_spec_add (hash_spec_t *hash_value,  enum SPEC_KEYS id, 
		          LINKED_LIST *ll_1, LINKED_LIST *ll_2, char *blob_item) {
  char *p;
  int retval = 1;

  /* if the hash lookup found something and the id's don't match
   * something is really wrong!
   */
  if (id != hash_value->id) {
    trace (ERROR, default_trace, "memory_hash_spec_add() error.  Attempt to add hash item with different id value-(key(%s),%d,%d)\n!", hash_value->key, id, hash_value->id);
    retval = -1;
  }

  /* there should be only one unique as-set or route-set object 
   * and therefore we need only one 'members:'/ll_1 list 
   */
  if (ll_1 != NULL && hash_value->items1 > 0) { 
    trace (ERROR, default_trace, "memory_hash_spec_add() error.  Attempt to append to existing 'members:' list!\n");
    trace (ERROR, default_trace, "----Object key-(%s) type (%d) # of items (%d) \n", hash_value->key, id, hash_value->items1);
    LL_Clear (hash_value->ll_1);
    hash_value->items1 = 0;
    hash_value->len1 = 0;
    retval = -1;
  }

  /* ditto above except 'mbrs_by_ref:'/ll_2 */
  if (ll_2 != NULL && hash_value->items2 > 0) {
    trace (ERROR, default_trace, "memory_hash_spec_add error.  Attempt to append to existing 'mbrs_by_ref:' list!\n");
    trace (ERROR, default_trace, "----Object key-(%s)\n", hash_value->key);
    LL_Clear (hash_value->ll_2);
    hash_value->items2 = 0; 
    hash_value->len2 = 0;
    retval = -1;
  }

  if (ll_1 != NULL) {
    LL_Iterate (ll_1, p) {
      LL_Add (hash_value->ll_1, new_irr_hash_string (p));
      hash_value->len1 += strlen (p) + 1;
      hash_value->items1++;
    }
  }

  if (ll_2 != NULL) {
    LL_Iterate (ll_2, p) {
      LL_Add (hash_value->ll_2,new_irr_hash_string (p));
      hash_value->len2 += strlen (p) + 1;
      hash_value->items2++;
    }
  }

  if (blob_item != NULL) {
    LL_Add (hash_value->ll_1, new_irr_hash_string (blob_item));
    hash_value->len1 += strlen (blob_item) + 1;
    hash_value->items1++;
  }

  return (retval);
}

hash_spec_t *memory_hash_spec_create (char *key, enum SPEC_KEYS id) {
  hash_spec_t *hash_value;
  irr_hash_string_t tmp;

  hash_value = New (hash_spec_t);
  hash_value->id = id;
  hash_value->key = strdup (key);

  hash_value->ll_1 = LL_Create (LL_Intrusive, True, 
				LL_NextOffset, LL_Offset (&tmp, &tmp.next),
				LL_PrevOffset, LL_Offset (&tmp, &tmp.prev),
				LL_DestroyFunction, delete_irr_hash_string, 
				0);
  hash_value->ll_2 = LL_Create (LL_Intrusive, True, 
				LL_NextOffset, LL_Offset (&tmp, &tmp.next),
				LL_PrevOffset, LL_Offset (&tmp, &tmp.prev),
				LL_DestroyFunction, delete_irr_hash_string, 
				0);

  return (hash_value);
}

int memory_hash_spec_remove (irr_database_t *db, char *key, enum SPEC_KEYS id,
                             char *blob_item) {
  hash_spec_t *hash_sval;

  convert_toupper(key);

  hash_sval = HASH_Lookup (db->hash_spec_tmp, key);

  if (hash_sval == NULL) { /* might be in the mem or disk hash index */
				    
    if ((hash_sval = fetch_hash_spec (db, key, UNPACK)) == NULL)
      return (-1); /* item not found, can't delete */

    HASH_Insert (db->hash_spec_tmp, hash_sval);
  }

  memory_hash_spec_del (hash_sval, id, blob_item);
  return (1);
} 
											  

int memory_hash_spec_store (irr_database_t *db, char *key, enum SPEC_KEYS id,
                            LINKED_LIST *ll_1, LINKED_LIST *ll_2, 
                            char *blob_item) {
  hash_spec_t *hash_sval;

  if (!strcmp (key, "@AS-CONCENTRIC")) {
    /*printf ("Here\n");*/
  }
  convert_toupper(key);

  hash_sval = HASH_Lookup (db->hash_spec_tmp, key);

  if (hash_sval == NULL) { /* might be in the mem or disk hash index */
				    
    if ((hash_sval = fetch_hash_spec (db, key, UNPACK)) == NULL)
      hash_sval = memory_hash_spec_create (key, id);

    HASH_Insert (db->hash_spec_tmp, hash_sval);
  }

  memory_hash_spec_add (hash_sval, id, ll_1, ll_2, blob_item);
  return(1);
} 
											  

/* this delete's the spec temp memory hash */
void Delete_hash_spec (hash_spec_t *hash_item) {

  if (hash_item == NULL)
    return;

  if (hash_item->key)
    Delete (hash_item->key);

  if (hash_item->ll_1)
    LL_Destroy (hash_item->ll_1);

  if (hash_item->ll_2)
    LL_Destroy (hash_item->ll_2);

#if (defined(USE_GDBM) || defined(USE_DB1))
  if (IRR.use_disk == 1) {
    if (hash_item->unpacked_value)
      Delete (hash_item->unpacked_value);
  }
#endif /* USE_GDBM || USE_DB1 */

  Delete (hash_item);
}


/* commit our mem index to our regular index for 2 reasons:
 * 1. the regular index is suitable for mem and disk storage
 *    ie, there are no linked lists.
 * 2. it is faster to manipulate mem indexes, so our disk
 *    scan processing will be faster and they need to be 
 *    written to disk anyway.
 */
void commit_spec_hash (irr_database_t *db) {
  hash_spec_t *hash_tval;

  HASH_Iterate (db->hash_spec_tmp, hash_tval) {
    if (hash_tval->items1 == 0 && hash_tval->items2 == 0)
      remove_hash_spec (db, hash_tval->key); 
    else 
      store_hash_spec (db, hash_tval); 
  }

}


/* makes the keys for the mbrs-by-ref hash 
 * used in the rpsl !i command
 */
void make_spec_key (char *new_key, char *maint, char *set_name) {
  
  /* some objects may not have a maintainer ;) */
  if (maint != NULL)
    strcpy (new_key, maint);
  else
    strcpy (new_key, "ANY");
  
  strcat (new_key, "|");
  strcat (new_key, set_name);
  convert_toupper (new_key);  
  
}

/* routine expects an origin without the "as", eg "231" */
void make_gas_key (char *gas_key, char *origin) {

  strcpy (gas_key, "@"); /* key uniqueness */
  strcat (gas_key, origin);
/*
  return (strdup (origin));
*/
}

/* make the has key for the (RIPE181) !h command */
void make_comm_key (char *new_key, char *community) {

  strcpy (new_key, "@"); /* this guarantee's key uniqueness */
  strcat (new_key, community);
  convert_toupper (new_key);
}

void make_setobj_key (char *new_key, char *obj_name) {

  strcpy (new_key, "@"); /* key uniqueness */
  strcat (new_key, obj_name);
  convert_toupper (new_key);
}
