
/*
 * $Id: indicies.c,v 1.2 2002/02/04 20:53:56 ljb Exp $
 * originally Id: indicies.c,v 1.38 1998/08/07 19:07:46 labovit Exp 
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

static int irr_hash_store (irr_database_t *database, char *key, char *value);

/* irr_database_store
 * 4 count | [2 type | 2 primary/secondary | 2 keylen | 4 offset | 4 len] | ...
 *
 */
int irr_database_store (irr_database_t *database, char *key, u_short p_or_s,
			enum IRR_OBJECTS type, u_long offset, u_long len) {
  hash_item_t *hash_item;
  char *buffer, *cp;
  u_short _keylen, _type = (u_short) type;
  u_int count, _size_old, _size_new;
  int ret_code = 1;

  _keylen = strlen (key);
  convert_toupper(key);
  hash_item = irr_database_fetch (database, key);

  /* JW want to dissallow duplicate primary key additions of same type */

  /* create a new hash entry */
  if (hash_item == NULL) {
    buffer = malloc (18);
    count = 0;
    cp = buffer + 4;
    _size_new = 18;
  }
  /* just append the new data to the end of the hash entry */
  else {
    cp = hash_item->value;
    UTIL_GET_NETLONG (count, cp);

    if ((count < 0) || (count > 1000)) {
      trace (ERROR, default_trace, 
	     "** WARN ** Suspiciously high count value (%d) for %s\n",
	     count, key);
      return -1;
    }

    cp = hash_item->value; /* reset ptr to keep things simple */

    _size_old = 4 + (count * 14);
    _size_new = _size_old + 14;

    buffer = malloc (_size_new);

    memcpy (buffer, cp, _size_old);
    cp = buffer + _size_old;
  } /* end append data */

  UTIL_PUT_NETSHORT (_type, cp); /* type */
  UTIL_PUT_NETSHORT (p_or_s, cp); /* primary/secondary */
  UTIL_PUT_NETSHORT (_keylen, cp); /* length of key  */
  UTIL_PUT_NETLONG (offset, cp); 
  UTIL_PUT_NETLONG (len, cp);

  cp = buffer;
  UTIL_PUT_NETLONG (++count, cp); /* count */

  if (hash_item != NULL) HASH_Remove (database->hash, hash_item);
  irr_hash_store (database, key, buffer);

  return ret_code; 
}



/* irr_hash_store
 * store indice in memory 
 */
static int irr_hash_store (irr_database_t *database, char *key, char *value) {
  hash_item_t *hash_item;

  hash_item = New (hash_item_t);
  hash_item->key = strdup (key);
  hash_item->value = value;
  HASH_Insert (database->hash, hash_item);
  return (1);
}

int irr_hash_destroy (hash_item_t *hash_item) {

  if (hash_item == NULL) 
    return (1);

  if (hash_item->key)
    Delete (hash_item->key);

  if (hash_item->value)
    Delete (hash_item->value);

  Delete (hash_item);

  return (1);
}

/* irr_database_fetch
 */
hash_item_t *irr_database_fetch (irr_database_t *database, char *key) {
  hash_item_t *hash_item;

  hash_item = HASH_Lookup (database->hash, key);
  return (hash_item);
}

/* irr_database_fetch
 * if dbm file use that, otherwise search memory 
 * 4 count | [2 type | 2 primary/secondary | 2 keylen | 4 offset | 4 len] | ...
 */
int irr_database_find_matches (irr_connection_t *irr, char *key, 
				   int p_or_s,
				   int match_behavior,
				   enum IRR_OBJECTS type,
				   u_long *ret_offset, u_long *ret_len) {
  irr_database_t *database;
  hash_item_t *hash_item = NULL;
  u_long offset, len;
  u_short _type, _p_or_s, _keylen;
  int count, exit_on_match = 0;
  char *cp;

  convert_toupper(key);

  LL_Iterate (irr->ll_database, database) {
    
    hash_item = irr_database_fetch (database, key);
    
    if (hash_item == NULL)
      continue;

    cp = hash_item->value;
    UTIL_GET_NETLONG (count, cp);
    
    while (count--) {
      _type = _p_or_s = _keylen = 0;
      UTIL_GET_NETSHORT (_type, cp);
      UTIL_GET_NETSHORT (_p_or_s, cp);
      UTIL_GET_NETSHORT (_keylen, cp);
      UTIL_GET_NETLONG (offset, cp);
      UTIL_GET_NETLONG (len, cp);

      /* exact match on keys, so maint-as195 != maintas1955 */
      if (strlen (key) != _keylen) 
	continue;

      /*
       * if (p_or_s != PRIMARY)
       *     if (_p_or_s != p_or_s)
       *      continue;
       *
       */
      
      /* check type */
      if ((match_behavior & TYPE_MODE) && (type != _type))
	continue;

      /* if ret_offset is not null, we're loading an object, so fill it in */
      if (ret_offset != NULL) {
	*ret_offset = offset;
	*ret_len = len;
	break;
      }
      irr_build_answer (irr, database->db_fp, _type, offset, len, NULL, database->db_syntax);

      /* RAWHOISD_MODE means exit after first the match */
      if (match_behavior & RAWHOISD_MODE) {
	exit_on_match = 1;
	break;
      }
    }
    if (exit_on_match)
      break;
  }

  return (1);
}

/* given an object key, find it's offset and length.  return 1 if found and
 * -1 otherwise.
 * can delete irr_database_load_object_seek() if this works
 */
int find_object_offset_len (irr_database_t *db, char *key, 
			    enum IRR_OBJECTS type,
			    u_long *offset, u_long *len) {
  irr_connection_t irr;
  int found = -1;
  
  *len = 0;
  irr.ll_database = LL_Create (0);

  LL_Add (irr.ll_database, db);
  irr_database_find_matches (&irr, key, PRIMARY, RAWHOISD_MODE|TYPE_MODE, type, 
			     offset, len);
  LL_Destroy (irr.ll_database);

  if (*len > 0)
    found = 1;
  
  return (found);
}


int irr_database_load_object_seek (irr_database_t *database, char *key, 
				   enum IRR_OBJECTS type,
				   irr_object_t *irr_object) {
  hash_item_t *hash_item;
  u_long offset, len;
  u_short _type, p_or_s, _keylen;
  int count;
  char *cp;

  convert_toupper(key);

  hash_item = irr_database_fetch (database, key);
  if (hash_item == NULL) return (-1);

  cp = hash_item->value;
  UTIL_GET_NETLONG (count, cp);
    
  while (count--) {
    UTIL_GET_NETSHORT (_type, cp);
    UTIL_GET_NETSHORT (p_or_s, cp);
    UTIL_GET_NETSHORT (_keylen, cp);
    UTIL_GET_NETLONG (offset, cp);
    UTIL_GET_NETLONG (len, cp);

    /* check type */
    if (type != _type)
      continue;

    /* exact key match, so as195 != as1955 */
    if (strlen (key) != _keylen) 
      continue;

    /* if object is not null, we're loading and object, so fill it in */
    if (irr_object != NULL) {
      irr_object->offset = offset;
      irr_object->len = len;
      return (1);
    }
  }

  return (-1);
}



/* irr_database_store
 * 4 count | [2 type | 2 primary/secondary | 2 keylen | 4 offset | 4 len] | ...
 *
 */
int irr_database_remove (irr_database_t *database, char *key, u_short p_or_s,
			 enum IRR_OBJECTS type, u_long offset, u_long len) {
  hash_item_t *hash_item;
  char *cp;
  char *buffer_new = NULL, *cp_new;
  u_short _type = (u_short) type;
  u_long _offset, _len, _p_or_s, _keylen;
  int count, size = 0, ret_code = 1;
  
  _offset = _len = _p_or_s = 0;

  convert_toupper (key);

  hash_item = irr_database_fetch (database, key);

  if (hash_item == NULL) 
    return -1;

  cp = hash_item->value;
  UTIL_GET_NETLONG (count, cp);

  if (--count > 0) {
  /*
    size = (4 + (count * 12) + 1);
  */
    size = 4 + (count * 14);
    cp_new = buffer_new = malloc (size);
    
    UTIL_PUT_NETLONG (count -1, cp_new);
    
    while (count--) {
      UTIL_GET_NETSHORT (_type, cp); /* type */
      UTIL_GET_NETSHORT (_p_or_s, cp); /* primary/secondary */
      UTIL_GET_NETSHORT (_keylen, cp); /* primary/secondary */
      UTIL_GET_NETLONG (_offset, cp); 
      UTIL_GET_NETLONG (_len, cp);
  
      if (offset == _offset) continue; /* skip this delete entry */

      UTIL_PUT_NETSHORT (_type, cp_new); /* type */
      UTIL_PUT_NETSHORT (_p_or_s, cp_new); /* primary/secondary */
      UTIL_PUT_NETSHORT (_keylen, cp_new); 
      UTIL_PUT_NETLONG (_offset, cp_new); 
      UTIL_PUT_NETLONG (_len, cp_new);
    }
  }  

/* JW: I think this section can be optimized later,
 * for case count > 0 just Delete(hash_item->value) and
 * hash_item->value = buffer_new;
 */
  HASH_Remove (database->hash, hash_item);
  if (count > 0)
    irr_hash_store (database, key, buffer_new);

  return ret_code;
}
