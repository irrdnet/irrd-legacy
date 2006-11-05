
/*
 * $Id: indicies.c,v 1.4 2002/10/17 20:02:30 ljb Exp $
 * originally Id: indicies.c,v 1.38 1998/08/07 19:07:46 labovit Exp 
 */

/* routines for handling indicies in hashes */

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

/* irr_database_store
 * 2 count | [1 type | 1 primary/secondary | 4 offset | 4 len] | ...
 */

#define OBJCOUNT_SIZE 2
#define OBJINFO_SIZE 10 

int irr_database_store (irr_database_t *database, char *key, u_char p_or_s,
			enum IRR_OBJECTS type, u_long offset, u_long len) {
  hash_item_t *hash_item;
  char *buffer, *cp;
  u_char _type = (u_char) type;
  u_int _size_old, _size_new;
  u_short count;
  int ret_code = 1;

  convert_toupper(key);
  hash_item = HASH_Lookup (database->hash, key);

  /* JW want to dissallow duplicate primary key additions of same type */

  /* create a new hash entry */
  if (hash_item == NULL) {
    _size_new = OBJCOUNT_SIZE + OBJINFO_SIZE;
    buffer = malloc (_size_new);
    count = 0;
    cp = buffer + OBJCOUNT_SIZE;
  }
  /* just append the new data to the end of the hash entry */
  else {
    cp = hash_item->value;
    UTIL_GET_NETSHORT (count, cp);

    if (count > 1000) {
      trace (ERROR, default_trace, 
     "%d entries for hash key %s exceeds 1000\n", count, key);
    }

    cp = hash_item->value; /* reset ptr to keep things simple */

    _size_old = OBJCOUNT_SIZE + (count * OBJINFO_SIZE);
    _size_new = _size_old + OBJINFO_SIZE;

    buffer = realloc (cp, _size_new);
    cp = buffer + _size_old;
  } /* end append data */

  *cp++ = _type;
  *cp++ = p_or_s;
  UTIL_PUT_NETLONG (offset, cp); 
  UTIL_PUT_NETLONG (len, cp);

  cp = buffer;
  count++;
  UTIL_PUT_NETSHORT (count, cp); /* count */

  if (hash_item == NULL) {
    hash_item = New (hash_item_t);
    hash_item->key = strdup (key);
    hash_item->value = buffer;
    HASH_Insert (database->hash, hash_item);
  } else
    hash_item->value = buffer;

  return ret_code; 
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

/* irr_database_find_matches
 * find matches and extract info from hash table entry
 * 2 count | [1 type | 1 primary/secondary | 4 offset | 4 len] | ...
 */
int irr_database_find_matches (irr_connection_t *irr, char *key, 
				   u_char p_or_s,
				   int match_behavior,
				   enum IRR_OBJECTS type,
				   u_long *ret_offset, u_long *ret_len) {
  irr_database_t *database;
  hash_item_t *hash_item = NULL;
  u_long offset, len;
  u_char _type, _p_or_s;
  u_short count;
  int exit_on_match = 0;
  char *cp;

  convert_toupper(key);

  if (match_behavior & RAWHOISD_MODE)
    exit_on_match = 1;

  LL_Iterate (irr->ll_database, database) {
    
    hash_item = HASH_Lookup (database->hash, key);
    
    if (hash_item == NULL)
      continue;

    cp = hash_item->value;
    UTIL_GET_NETSHORT (count, cp);
    
    while (count--) {
      _type = *cp++;
      _p_or_s = *cp++;
      UTIL_GET_NETLONG (offset, cp);
      UTIL_GET_NETLONG (len, cp);

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
      irr_build_answer (irr, database, _type, offset, len);

      /* RAWHOISD_MODE means exit after first the match */
      if (exit_on_match)
	break;
    }
    if (exit_on_match)
      break;
  }
  return (1);
}

/* given an object key, find it's offset and length.  return 1 if found and
 * -1 otherwise.
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

/* irr_database_remove
 * 2 count | [1 type | 1 primary/secondary | 4 offset | 4 len] | ...
 *
 */
int irr_database_remove (irr_database_t *database, char *key, u_long offset) {
  hash_item_t *hash_item;
  char *cp;
  char *buffer_new = NULL;
  u_long _offset;
  u_short count, new_count;
  
  convert_toupper (key);
  hash_item = HASH_Lookup (database->hash, key);

  if (hash_item == NULL)  {
    trace (ERROR, default_trace, 
   "irr_database_remove(): unable to find hash for key %s\n", key);
    return -1;
  }

  cp = hash_item->value;
  UTIL_GET_NETSHORT (count, cp);

  if (count == 0) {
      trace (ERROR, default_trace, 
     "irr_database_remove(): count value (%d) for hash key %s is <= 0\n", count, key);
      return -1;
  }

  new_count = count - 1;
  if (new_count > 0) {
    int found = 0;

    while (!found) {
      cp += 2;	/* skip over type and primary/secondary flag */
      UTIL_GET_NETLONG (_offset, cp); 
      cp += 4;	/* skip length field */
      count--; 
      if (offset == _offset) found = 1; /* found the entry */
      if (count == 0) break;	/* we have scanned all the entries */
    }
    if (!found) {
      trace (ERROR, default_trace, "irr_database_remove(): did not find entry for key: %s, at offset: %d\n", key, offset);
      return -1;
    }
    if (count > 0) {  /* see if we need to shift down entries */
      memmove(cp - OBJINFO_SIZE, cp, count * OBJINFO_SIZE);
    }
    cp = hash_item->value;  /* point back to beginning of buffer */
    UTIL_PUT_NETSHORT (new_count, cp); /* store new count */
    buffer_new = realloc(hash_item->value, OBJCOUNT_SIZE + (new_count * OBJINFO_SIZE)); /* free unused space */
    hash_item->value = buffer_new;
  } else {
    HASH_Remove (database->hash, hash_item);
  }

  return 1;
}
