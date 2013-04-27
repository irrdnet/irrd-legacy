/*
 * $Id: hash_spec.c,v 1.4 2002/10/17 20:02:30 ljb Exp $
 * originally Id: hash_spec.c,v 1.14 1998/07/20 01:22:03 labovit Exp 
 */

/* routines for handling special indicies in hashes  */

#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <fcntl.h>
#include <ctype.h>
#include <glib.h>

#include "mrt.h"
#include "trace.h"
#include "config_file.h"
#include "irrd.h"

void commit_spec_hash_process(gpointer key, hash_spec_t *hash_tval, irr_database_t *db);

/* called when indexes are stored in main memory hash */
int irr_spec_hash_store (irr_database_t *database, char *key, char *value) {
  hash_item_t *hash_item;

  hash_item = irrd_malloc(sizeof(hash_item_t));
  hash_item->key = strdup (key);
  hash_item->value = value;
  g_hash_table_insert(database->hash_spec, hash_item->key, hash_item);
  return (1);
}
  
/* put a series of object offsets/lengths from a linked list */
void util_put_ll_objs (LINKED_LIST *ll, char **cp) {
  objlist_t *obj_p;
  u_short _type;
  char *c = *cp;

  LL_Iterate (ll, obj_p) {
    UTIL_PUT_NETLONG  (obj_p->offset, c); 
    UTIL_PUT_NETLONG  (obj_p->len, c);
    _type = (u_short) obj_p->type;
    UTIL_PUT_NETSHORT (_type, c); /* type */
  }
  *cp = c;
}

/* make a string from a linked list, each item is seperated by a ' ' */
void util_put_ll_string (LINKED_LIST *ll, char **cp) {
  irr_hash_string_t *p;
  char *c = *cp;

  LL_Iterate (ll, p) {
    strcpy (c, p->string);
    while (*++c); 
    *c++ = ' ';
  }
  *c++ = '\0';  /* terminate with a null byte */
  *cp = c;	/* update pointer past the null */
}

/* make a linked list of objects from a string */
void util_get_ll_objs (LINKED_LIST **ll, u_long items, char **cp) {
  objlist_t *obj_p;
  u_short _type;
  char *c = *cp;

  (*ll) = LL_Create (LL_DestroyFunction, free, 0);

  while (items > 0) {
    obj_p = irrd_malloc(sizeof(objlist_t));
    UTIL_GET_NETLONG (obj_p->offset, c);
    UTIL_GET_NETLONG (obj_p->len, c);
    UTIL_GET_NETSHORT (_type, c);
    obj_p->type = _type;
    LL_Add ((*ll), obj_p);
    items--;
  }
  *cp = c; 
}

/* make a linked list from a string, items in the string are seperated by a ' ' */
int util_get_ll_string (LINKED_LIST **ll, u_long items, char **cp) {
  char *a, *b;
  irr_hash_string_t tmp;
  int return_len;

  a = *cp;

  (*ll) = LL_Create (LL_Intrusive, True, 
		     LL_NextOffset, LL_Offset (&tmp, &tmp.next),
		     LL_PrevOffset, LL_Offset (&tmp, &tmp.prev),
		     LL_DestroyFunction, delete_irr_hash_string, 0);

  if (items == 0)
    return (0);

  while (items > 0) {
    if ((b = strchr (a, ' ')) == NULL) {
      /* this should't happen */
      trace (ERROR, default_trace, "util_get_ll_string: badly formatted list - %s;\n", *cp);
      break;
    }
    *b = '\0';
    LL_Add ((*ll), new_irr_hash_string (a));
    *b++ = ' ';
    a = b;
    items--;
  }
  return_len = a - *cp;	/* calculate the length of this string */
  /* make cp point to the begining of the next item (skip null) */
  *cp = a + 1; 
  return (return_len);
}

/* Marshal the hash_spec_t struct into a hash_item_t struct.
 * this means flattening out the linked lists (ie, put both
 * ll's into a single char string).
 */
void store_hash_spec (irr_database_t *database, hash_spec_t *hash_sval) { 
  char *cp, *buf;
  u_short _id;
  u_int str_size;
  hash_item_t *hash_x;

  _id = hash_sval->id;

  /* compute the size of the packed value string */
  /* the lengths should include the '\0' at the end */
  if (_id == MNTOBJS )
    str_size = NETSHORT_SIZE + NETLONG_SIZE + hash_sval->items2 * (2 * NETLONG_SIZE + NETSHORT_SIZE);
  else {
    str_size = NETSHORT_SIZE + NETLONG_SIZE + hash_sval->len1 + 1;
    if (_id == SET_OBJX)
      str_size += NETLONG_SIZE + hash_sval->len2 + 1;
/* XXX right for GASX6? */
    else if (_id == GASX || _id == GASX6 || _id == SET_MBRSX)
      str_size += NETLONG_SIZE + hash_sval->items2 * (2 * NETLONG_SIZE + NETSHORT_SIZE);
  }

  /* now pack up the value part */
  cp = buf = malloc (str_size);
  UTIL_PUT_NETSHORT (_id, cp); 
  if (_id == MNTOBJS ) {
    UTIL_PUT_NETLONG  (hash_sval->items2, cp);
    util_put_ll_objs(hash_sval->ll_2, &cp);
  } else {
    UTIL_PUT_NETLONG  (hash_sval->items1, cp);
    if (hash_sval->items1 > 0)
      util_put_ll_string(hash_sval->ll_1, &cp);
    if (_id == SET_OBJX) {
      UTIL_PUT_NETLONG  (hash_sval->items2, cp);
      if (hash_sval->items2 > 0)
        util_put_ll_string(hash_sval->ll_2, &cp);
/* XXX right for GASX6? */
    } else if (_id == GASX || _id == GASX6 || _id == SET_MBRSX) {
      UTIL_PUT_NETLONG  (hash_sval->items2, cp);
      if (hash_sval->items2 > 0)
        util_put_ll_objs(hash_sval->ll_2, &cp);
    }
  }

  hash_x = g_hash_table_lookup(database->hash_spec, hash_sval->key);
  if (hash_x != NULL) g_hash_table_remove(database->hash_spec, hash_x->key);
  irr_spec_hash_store (database, hash_sval->key, buf);
}

void remove_hash_spec (irr_database_t *db, char *key) {
  /* printf("enter remove_hash_spec( key-(%s))\n",key); */
  g_hash_table_remove(db->hash_spec, key);
}

hash_spec_t *fetch_hash_spec (irr_database_t *database, char *key, 
                              enum FETCH_T mode) {
  char *cp;
  u_short _id;
  hash_item_t *hash_item = NULL;
  hash_spec_t *hash_sval = NULL;

  hash_item = g_hash_table_lookup(database->hash_spec, key);

  if (hash_item) {
    cp = hash_item->value;
    hash_sval = irrd_malloc(sizeof(hash_spec_t));
    hash_sval->key = strdup (key);
    UTIL_GET_NETSHORT (_id, cp);
    hash_sval->id = _id;
    if (_id == MNTOBJS) {
        UTIL_GET_NETLONG (hash_sval->items2, cp);
        util_get_ll_objs (&hash_sval->ll_2, hash_sval->items2, &cp);
    } else {
      UTIL_GET_NETLONG (hash_sval->items1, cp);
      if (mode == UNPACK) {
        hash_sval->len1 = util_get_ll_string (&hash_sval->ll_1, 
                   hash_sval->items1, &cp);
        if (_id == SET_OBJX) {
          UTIL_GET_NETLONG (hash_sval->items2, cp);
          hash_sval->len2 = util_get_ll_string (&hash_sval->ll_2,
                   hash_sval->items2, &cp);
/* XXX right for GASX6? */
        } else if (_id == GASX || _id == GASX6 || _id == SET_MBRSX) {
          UTIL_GET_NETLONG (hash_sval->items2, cp);
          util_get_ll_objs (&hash_sval->ll_2, hash_sval->items2, &cp);
        }
      } else  {/* FAST mode, just return the unpacked item, ie !gas */
        if (hash_sval->items1 == 0)
          hash_sval->len1 = 0;
        else { 
          hash_sval->len1 =  strlen(cp);
          hash_sval->gas_answer = cp; /* points to the first string/gas answer */
        }
      }
    }
  }    
  else
    hash_sval = NULL;

  return (hash_sval);
}

void memory_hash_spec_del (hash_spec_t *hash_value, enum SPEC_KEYS id, 
                            irr_object_t *irr_object) {
  irr_hash_string_t *irr_hash_str;
  objlist_t *obj_p;

  switch (id) {
    case SET_OBJX:
      hash_value->len1 = hash_value->len2 = 0; 
      hash_value->items1 = hash_value->items2 = 0;
      LL_Clear (hash_value->ll_1);
      LL_Clear (hash_value->ll_2);
      break;
    case GASX:
    case GASX6:
    case SET_MBRSX:
      if (irr_object->name == NULL)
        return;
      if ( (irr_object->type == ROUTE && id == GASX) ||
    	   (irr_object->type == ROUTE6 && id == GASX6) ||
	   (irr_object->type != ROUTE6 && id == SET_MBRSX) ) {
        LL_Iterate (hash_value->ll_1, irr_hash_str) {
          if (!strcmp (irr_object->name, irr_hash_str->string)) {
            LL_Remove (hash_value->ll_1, irr_hash_str);
            hash_value->items1--;
            hash_value->len1 -= strlen (irr_object->name) + 1; 
            break;
          }
        }
      }
      /* fall-through */
    case MNTOBJS:
      LL_Iterate (hash_value->ll_2, obj_p) {
        if (irr_object->offset == obj_p->offset) {
          LL_Remove (hash_value->ll_2, obj_p);
          hash_value->items2--;
          break;
        }
      }
      break;
    default:
      trace (ERROR, default_trace, "memory_hash_spec_del() error.  Got an unknown index id-type, cannot perform delete operation id-(%d)\n!", id);
      break;
  };
}

hash_spec_t *memory_hash_spec_create (char *key, enum SPEC_KEYS id) {
  hash_spec_t *hash_value;
  irr_hash_string_t hash_str;

  hash_value = irrd_malloc(sizeof(hash_spec_t));
  hash_value->id = id;
  hash_value->key = strdup (key);

  if (id == MNTOBJS) {
    hash_value->ll_1 = NULL;
    hash_value->ll_2 = LL_Create (LL_DestroyFunction, free, 0);
  } else {
    hash_value->ll_1 = LL_Create (LL_Intrusive, True, 
			LL_NextOffset, LL_Offset (&hash_str, &hash_str.next),
			LL_PrevOffset, LL_Offset (&hash_str, &hash_str.prev),
			LL_DestroyFunction, delete_irr_hash_string, 0);
  
    if (id == SET_OBJX) {
      hash_value->ll_2 = LL_Create (LL_Intrusive, True, 
			LL_NextOffset, LL_Offset (&hash_str, &hash_str.next),
			LL_PrevOffset, LL_Offset (&hash_str, &hash_str.prev),
			LL_DestroyFunction, delete_irr_hash_string, 0);
    } else if (id == GASX || id == GASX6 || id == SET_MBRSX) {
      hash_value->ll_2 = LL_Create (LL_DestroyFunction, free, 0);
    }
  }

  return (hash_value);
}

int memory_hash_spec_remove (irr_database_t *db, char *key, enum SPEC_KEYS id,
                            irr_object_t *irr_object) {
  hash_spec_t *hash_sval;

  convert_toupper(key);
  hash_sval = g_hash_table_lookup(db->hash_spec_tmp, key);

  if (hash_sval == NULL) { /* might be in the mem hash index */
				    
    if ((hash_sval = fetch_hash_spec (db, key, UNPACK)) == NULL)
      return (-1); /* item not found, can't delete */

    g_hash_table_insert(db->hash_spec_tmp, hash_sval->key, hash_sval);
  }

  memory_hash_spec_del (hash_sval, id, irr_object);
  return (1);
} 

int memory_hash_spec_store (irr_database_t *db, char *key, enum SPEC_KEYS id,
                            irr_object_t *irr_object) {
  hash_spec_t *hash_sval;
  LINKED_LIST *ll_1 = NULL;
  LINKED_LIST *ll_2 = NULL;
  char *tmpstr;
  objlist_t *obj_p;
  int retval = 1;

  convert_toupper(key);
  hash_sval = g_hash_table_lookup(db->hash_spec_tmp, key);

  if (hash_sval == NULL) { /* might be in the mem hash index */
				    
    if ((hash_sval = fetch_hash_spec (db, key, UNPACK)) == NULL)
      hash_sval = memory_hash_spec_create (key, id);

    g_hash_table_insert(db->hash_spec_tmp, hash_sval->key, hash_sval);
  }

if (id == GASX6)
  /* if the hash lookup found something and the id's don't match
   * something is really wrong!
   */
  if (id != hash_sval->id) {
    trace (ERROR, default_trace, "Attempt to add hash item with different id value-(key(%s),%d,%d)\n!", hash_sval->key, id, hash_sval->id);
    return (-1);
  }

  switch (id) {
    case SET_OBJX:
      if ( (ll_1 = irr_object->ll_mbrs) != NULL) {
        if (hash_sval->items1 > 0) { 
          /* there should be only one unique as-set or route-set object 
           * and therefore we need only one 'members:'/ll_1 list 
           */
          trace (ERROR, default_trace, "Append to existing 'members:' list! Object key-(%s)\n", hash_sval->key);
          LL_Clear (hash_sval->ll_1);
          hash_sval->items1 = 0;
          hash_sval->len1 = 0;
          retval = -1;
        }
        LL_Iterate (ll_1, tmpstr) {
          LL_Add (hash_sval->ll_1, new_irr_hash_string (tmpstr));
          hash_sval->len1 += strlen (tmpstr) + 1;
          hash_sval->items1++;
        }
      }
      if ( (ll_2 = irr_object->ll_mbr_by_ref) != NULL) {
        if (hash_sval->items2 > 0) {
          /* ditto above except 'mbrs_by_ref:'/ll_2 */
          trace (ERROR, default_trace, "Append to existing 'mbrs_by_ref:' list! Object key-(%s)\n", hash_sval->key);
          LL_Clear (hash_sval->ll_2);
          hash_sval->items2 = 0; 
          hash_sval->len2 = 0;
          retval = -1;
	}
	LL_Iterate (ll_2, tmpstr) {
	  LL_Add (hash_sval->ll_2,new_irr_hash_string (tmpstr));
	  hash_sval->len2 += strlen (tmpstr) + 1;
 	  hash_sval->items2++;
	}
      }
      break;
    case GASX:
    case GASX6:
    case SET_MBRSX:
      if ( (tmpstr = irr_object->name) == NULL )
 	return(retval);
      if ( (irr_object->type == ROUTE && id == GASX) ||
	   (irr_object->type == ROUTE6 && id == GASX6) ||
	   (irr_object->type != ROUTE6 && id == SET_MBRSX) ) {
        LL_Add (hash_sval->ll_1, new_irr_hash_string (tmpstr));
        hash_sval->len1 += strlen (tmpstr) + 1;
        hash_sval->items1++;
      }
      /* fall through */
    case MNTOBJS:
      obj_p = irrd_malloc(sizeof(objlist_t));
      obj_p->offset = irr_object->offset;
      obj_p->len = irr_object->len;
      obj_p->type = irr_object->type;
      LL_Add (hash_sval->ll_2, obj_p);
      hash_sval->items2++;
      break;
  }

  return(retval);
} 

/* this delete's the spec temp memory hash */
void Delete_hash_spec (hash_spec_t *hash_sval) {

  if (hash_sval == NULL)
    return;

  if (hash_sval->key)
    irrd_free(hash_sval->key);

  if (hash_sval->ll_1)
    LL_Destroy (hash_sval->ll_1);

  if (hash_sval->ll_2)
    LL_Destroy (hash_sval->ll_2);

  irrd_free(hash_sval);
}

/* commit updated index
 */
void commit_spec_hash (irr_database_t *db) {
  g_hash_table_foreach(db->hash_spec_tmp, (GHFunc)commit_spec_hash_process, db);
}

/* This is needed for older versions of glib (before 2.6) that
 * don't support iteration in hash tables 
 */
void commit_spec_hash_process(gpointer key, hash_spec_t *hash_tval, irr_database_t *db) {
    if (hash_tval->items1 == 0 && hash_tval->items2 == 0)
      remove_hash_spec (db, hash_tval->key); 
    else 
      store_hash_spec (db, hash_tval); 
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

/* makes the keys for the mbrs objects hash 
 */
void make_mntobj_key (char *new_key, char *maint) {

  *new_key++ = '|'; /* key uniqueness */
  strcpy (new_key, maint);
  convert_toupper (new_key);
}

/* routine expects an origin without the "as", eg "231" */
void make_gas_key (char *gas_key, char *origin) {
  char *tmpptr;

  *gas_key++ = '@'; /* key uniqueness */
  if ((tmpptr = index(origin,'.')) != NULL) {
    char  asplain[BUFSIZE];

    *tmpptr = 0;
    sprintf(asplain,"%u", atoi(origin)*65536 + atoi(tmpptr + 1));
    *tmpptr = '.';
    strcpy (gas_key, asplain);
  } else {
    strcpy (gas_key, origin);
  }
}

/* routine expects an origin without the "as", eg "231" */
void make_6as_key (char *gas_key, char *origin) {
  char *tmpptr;

  *gas_key++ = '%'; /* key uniqueness */

  if ((tmpptr = index(origin,'.')) != NULL) {
    char  asplain[BUFSIZE];
  
    *tmpptr = 0;
    sprintf(asplain,"%u", atoi(origin)*65536 + atoi(tmpptr + 1));
    *tmpptr = '.';
    strcpy (gas_key, asplain);
  } else {
    strcpy (gas_key, origin);
  }
}

void make_setobj_key (char *new_key, char *obj_name) {

  *new_key++ = '@'; /* key uniqueness */
  strcpy (new_key, obj_name);
  convert_toupper (new_key);
}
