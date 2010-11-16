/*
 * $Id: rpsl_commands.c,v 1.10 2002/10/17 20:02:31 ljb Exp $
 * originally Id: rpsl_commands.c,v 1.22 1998/07/31 15:42:39 gerald Exp
 */

#include <stdio.h>
#include <string.h>
#include "mrt.h"
#include "trace.h"
#include <time.h>
#include <signal.h>
#include "config_file.h"
#include <sys/types.h>
#include <ctype.h>
#include "hash.h"
#include "stack.h"
#include "array.h"
#include <fcntl.h>
#include "irrd.h"

extern trace_t *default_trace;
extern irr_t IRR;
/* a list of prefix range types */
enum EXPAND_TYPE { NO_EXPAND, ROUTE_SET_EXPAND, OTHER_EXPAND };
typedef struct _member_examined_hash_t {
  char *key;
} member_examined_hash_t;

/* local routines */
void update_members_list (irr_database_t *database, char *range_op,
			enum EXPAND_TYPE expand_flag, 
			HASH_TABLE  *hash_member_examined, 
			LINKED_LIST *ll_setlist, LINKED_LIST *ll_set_names, 
			STACK *stack, irr_connection_t *irr);
void mbrs_by_ref_set (irr_database_t *database, char *range_op,
	enum EXPAND_TYPE expand_flag, LINKED_LIST *ll_setlist,
	char *set_name, LINKED_LIST *ll_mbr_by_ref, irr_connection_t *irr);
char *rpsl_macro_expand_add (char *range, char *name,
				irr_connection_t *irr, char *dbname);
void SL_Add (LINKED_LIST *ll_setlist, char *member, char *range_op, enum EXPAND_TYPE expand_flag, irr_connection_t *irr);
int chk_set_name (char *);

void HashMemberExaminedDestroy(member_examined_hash_t *h) {
  if (h == NULL) return;
  if (h->key) Delete (h->key);
  Delete(h);
}

/* as-set/route-set expansion !ias-bar */
void irr_set_expand (irr_connection_t *irr, char *name) {
  irr_database_t *database;
  time_t start_time;
  DATA_PTR *array;
  STACK *stack;
  HASH_TABLE *hash_member_examined;
  member_examined_hash_t member_examined_item;
  member_examined_hash_t *member_examined_ptr;
  LINKED_LIST *ll_setlist;
  char *set_name, *last_set_name, *mstr, *db;
  char *lasts, *range_op, abuf[BUFSIZE];
  int i, first, dup, expand_flag = NO_EXPAND;
  hash_spec_t *hash_spec;

  if (strchr(name, ',') != NULL) {
    strtok_r(name, ",", &lasts);
    /* check if we are expanding a route-set */
    if ( (set_name = strchr(name, ':')) != NULL)
      set_name++;
    else
      set_name = name;
    if (!strncasecmp (set_name, "rs-", 3))
      expand_flag = ROUTE_SET_EXPAND;
    else
      expand_flag = OTHER_EXPAND;
  }

  start_time = time(NULL);
  convert_toupper (name);
  stack = Create_STACK (1024);
  hash_member_examined = HASH_Create(SMALL_HASH_SIZE, HASH_KeyOffset,
       HASH_Offset(&member_examined_item, &member_examined_item.key),
       HASH_DestroyFunction, HashMemberExaminedDestroy, 0);
  ll_setlist = LL_Create (LL_DestroyFunction, free, NULL); 
  mstr = rpsl_macro_expand_add (" ", name, irr, NULL);
  Push (stack, mstr);
  member_examined_ptr = New(member_examined_hash_t);
  member_examined_ptr->key = strdup(name);
  HASH_Insert(hash_member_examined, member_examined_ptr);

  while (Get_STACK_Count (stack) > 0) {
    if ( IRR.expansion_timeout > 0 ) {
      if ( (time(NULL) - start_time) > IRR.expansion_timeout ) {
        trace (ERROR, default_trace, "irr_set_expand(): Set expansion timeout\n");
        sprintf(abuf, "Expansion maximum CPU time exceeded: %d seconds", IRR.expansion_timeout);
        irr_send_error(irr, abuf);
        goto getout;
      }
    }
    mstr = (char *) Pop (stack);
    /* might want to check the examined list to see if this set name
       has been examined already */
    first = 1;
    lasts = NULL;
    range_op = strtok_r (mstr, ",", &lasts);
    if (!strcmp (range_op, " ")) range_op = NULL;
    set_name = strtok_r (NULL, ",", &lasts);

    irr_lock_all(irr); /* lock db's while searching */
    while ((db = strtok_r (NULL, ",", &lasts)) != NULL) {
      if ((database = find_database (db)) == NULL) {
        trace (ERROR, default_trace, "irr_set_expand(): Database not found %s\n", db);
        sprintf(abuf, "Database not found: %s", db);
        irr_send_error(irr, abuf);
        goto getout;
      }
      make_setobj_key (abuf, set_name);
      if ((hash_spec = fetch_hash_spec (database, abuf, UNPACK)) != NULL) {

          first = 0; 
	  update_members_list (database, range_op, expand_flag, hash_member_examined, 
			       ll_setlist, hash_spec->ll_1, stack, irr);
	  mbrs_by_ref_set (database, range_op, expand_flag, ll_setlist, 
                              set_name, hash_spec->ll_2, irr);
	  Delete_hash_spec (hash_spec); 
      }
      if (first == 0)
        break;
    }
    irr_unlock_all(irr);
    free (mstr);
  }

  first = 1;
  dup = 0;
  i = 0;
  last_set_name = "";
  array = NewArray(DATA_PTR, ll_setlist->count);
  LL_ContIterate (ll_setlist, set_name) {
    array[i++] = set_name;
  }
  ARRAY_Sort(array, ll_setlist->count, strcmp);
  i = 0;
  while (i < ll_setlist->count) {
    set_name = array[i++];
    if (!first) {
       /* since list is sorted, any duplicates should be consecutive */
      if (strcmp (last_set_name, set_name) == 0)
        dup = 1;
      else /* add a space before each item */
        irr_add_answer (irr, " ");
    }
    if (!dup) { /* only print this if not a duplicate */
      irr_add_answer (irr, "%s", set_name);
      last_set_name = set_name;
    } else
      dup = 0;
    first = 0;
  }
  Delete(array);
  irr_send_answer (irr);
getout:
  LL_Destroy (ll_setlist);
  HASH_Destroy (hash_member_examined);
  Destroy_STACK (stack);
}

void mbrs_by_ref_set (irr_database_t *database, char *range_op,
	enum EXPAND_TYPE expand_flag, LINKED_LIST *ll_setlist,
	char *set_name, LINKED_LIST *ll_mbr_by_ref, irr_connection_t *irr) {
  char *member, *maint, key[BUFSIZE];
  hash_spec_t *hash_spec;

  if (ll_mbr_by_ref == NULL) 
    return;

  if (expand_flag == NO_EXPAND) {
    LL_ContIterate (ll_mbr_by_ref, member) {
      LL_Add(ll_setlist, strdup (member));
    }
    return;
  }

  /* Find all the route or autnum's which reference the set name 
   * (via 'members-of:').
   */
  LL_ContIterate (ll_mbr_by_ref, maint) {
    make_spec_key (key, maint, set_name);
    if ((hash_spec = fetch_hash_spec (database, key, UNPACK)) != NULL) {
      LL_ContIterate (hash_spec->ll_1, member) {
	SL_Add (ll_setlist, member, range_op, expand_flag, irr);
      }
      Delete_hash_spec (hash_spec);
    }
  }
}

void update_members_list (irr_database_t *database, char *range_op, enum EXPAND_TYPE expand_flag, 
			  HASH_TABLE *hash_member_examined, 
			  LINKED_LIST *ll_setlist, LINKED_LIST *ll_set_names, 
			  STACK *stack, irr_connection_t *irr) {
  char *member, *p, *r;
  char buffer[BUFSIZE], range_buf[512];
  int len = 0;
  member_examined_hash_t *member_examined_ptr;

  if (ll_set_names == NULL)
    return;

  LL_ContIterate (ll_set_names, member) {
    if ((expand_flag == NO_EXPAND) || !chk_set_name (member)) {
      SL_Add (ll_setlist, member, range_op, expand_flag, irr);
    } else { /* we have a set name */
      if (!HASH_Lookup(hash_member_examined, member)) {
	strcpy(range_buf, " "); /* initialize to empty range */
	r = member;
	/* Need to seperate the range op from the set 
	   name for rpsl_macro_expand_add() */
	if ((p = strchr (member, '^')) != NULL) {
	  strncpy (buffer, member, p - member);
	  buffer[p - member] = '\0';
          len = strlen(p);
	  if (len < 512) /* don't overflow buffer */
	     strcpy(range_buf,p);
	  r = buffer; /* this is the set name without the range op */
        } 
        if (range_op != NULL) {	  /* append existing range_op */
          if ( (len + strlen(range_op)) < 512 ) {
            if (p == NULL)
	      range_buf[0] = '\0';
	    strcat(range_buf, range_op);
          }
        }
        p = rpsl_macro_expand_add (range_buf, r, irr, database->name); 
        Push (stack, p);
        member_examined_ptr = New(member_examined_hash_t);
        member_examined_ptr->key = strdup(r);
        HASH_Insert(hash_member_examined, member_examined_ptr);
      }
    }
  } 
} /* void update_members_list() */

/* a list of prefix range types */
enum PREFIX_RANGE_TYPE {
  INVALID_RANGE, EXCLUSIVE_RANGE, INCLUSIVE_RANGE, VALUE_RANGE };

enum PREFIX_RANGE_TYPE prefix_range_parse( char *range, unsigned int *start, unsigned int *end ) {
  char *p;

  p = range;
  if (*p == '+')
    return INCLUSIVE_RANGE;
  if (*p == '-')
    return EXCLUSIVE_RANGE;
  if (strchr(p, '-') != NULL) {
    if (sscanf(p, "%u-%u", start, end) != 2)
      return INVALID_RANGE;
    else
      return VALUE_RANGE;
  }
  if (sscanf(p, "%u", start) != 1)
    return INVALID_RANGE;
  else {
    *end = *start;
    return VALUE_RANGE;
  }
}
 
/*
 * Sorted and unique linked list add.  *p is the potential item to be added.
 * Also appends a range operator to *p.
 *
 */
void SL_Add (LINKED_LIST *ll_setlist, char *member, char *range_op, enum EXPAND_TYPE expand_flag, irr_connection_t *irr) {
  char buffer[BUFSIZE], rangestr[32];
  char *q, *temp_ptr, *last, *range_ptr;
  hash_spec_t *hash_spec;
  irr_database_t *db;
  unsigned int bitlen;
  enum  PREFIX_RANGE_TYPE range_op_type, prefix_range_type;
  unsigned int range_op_start, range_op_end, prefix_range_start, prefix_range_end;

  /* if performing a route-set expansion, check for AS numbers and lookup route prefixes which list the AS as their origin */
  if ( expand_flag == ROUTE_SET_EXPAND && !strncasecmp(member, "AS", 2)) {
    make_gas_key(buffer, member + 2);
    LL_ContIterate (irr->ll_database, db) { /* search over all databases */
      if ((hash_spec = fetch_hash_spec(db, buffer, FAST)) != NULL) {
        if (hash_spec->len1 > 0) {
          q = strdup(hash_spec->gas_answer);
          temp_ptr = strtok_r(q, " ", &last);
	  while (temp_ptr != NULL && *temp_ptr != '\0') {
	    SL_Add(ll_setlist, temp_ptr, range_op, expand_flag, irr);
	    temp_ptr = strtok_r(NULL, " ", &last);
	  }
	  free(q);
          Delete_hash_spec(hash_spec);
        }
      }
    }
    return;
  }

  strcpy (buffer, member);
  if (range_op == NULL)
    goto add_member;
  if (*range_op != '^') {	/* should start with a '^' */
    trace (ERROR, default_trace, "SL_Add(): range_op does not start with a '^' : %s\n", range_op);
    goto add_member;
  }
  range_ptr = strdup(range_op + 1);
  q = strtok_r(range_ptr, "^", &last);
  while (q != NULL && *q != '\0') {
    range_op_type = prefix_range_parse(q, &range_op_start, &range_op_end);
    if (range_op_type == INVALID_RANGE) {
      trace (ERROR, default_trace, "SL_Add(): range_op is invalid : %s\n", range_op);
      return;
    }
    /* should have a prefix length to be legal */
    if ( (temp_ptr = strchr(member, '/')) != NULL )
      bitlen = atoi(temp_ptr + 1);
    else {
      trace (ERROR, default_trace, "SL_Add(): prefix missing: %s\n", member);
      free(range_ptr);
      return;
    }

    if ( (temp_ptr = strchr(buffer, '^')) == NULL ) /* check for range on prefix */
      prefix_range_start = (prefix_range_end = bitlen);
    else {
      prefix_range_type = prefix_range_parse(temp_ptr + 1, &prefix_range_start, &prefix_range_end);
      if (prefix_range_type == INVALID_RANGE) {
        free(range_ptr);
        trace (ERROR, default_trace, "SL_Add(): prefix range is invalid : %s\n", buffer);
        return;
      }
      *temp_ptr = 0;	/* terminate string at range */
      if (prefix_range_type == EXCLUSIVE_RANGE) {
	prefix_range_start = bitlen + 1;
	prefix_range_end = 32;
      } else if (prefix_range_type == INCLUSIVE_RANGE) {
	prefix_range_start = bitlen;
	prefix_range_end = 32;
      }
    }

    if (range_op_type == INCLUSIVE_RANGE) {
	range_op_start = prefix_range_start;
	range_op_end = 32;
    } else if (range_op_type == EXCLUSIVE_RANGE) {
	range_op_start = prefix_range_start + 1;
	range_op_end = 32;
    } else if (range_op_type == VALUE_RANGE) {
	if (range_op_end < prefix_range_start) {
          free(range_ptr);
	  return;  /* apply an less specific range to a more specific one */
	}
	if (prefix_range_start > range_op_start)
	  range_op_start = prefix_range_start;
    }

    if (range_op_start > range_op_end) {
        free(range_ptr);
	return; /* specific range exceeds maximum, toss prefix */
    }
    if ( (bitlen == range_op_start) && (bitlen == range_op_end) )
	strcpy(rangestr,"");
    else if (range_op_end == range_op_start)
	sprintf(rangestr,"^%d", range_op_start);
    else if (range_op_end == 32 && range_op_start == bitlen)
	strcpy(rangestr,"^+");
    else if (range_op_end == 32 && range_op_start == (bitlen + 1))
	strcpy(rangestr,"^-");
    else
	sprintf(rangestr,"^%d-%d",range_op_start, range_op_end);
    strcat(buffer, rangestr);
    q = strtok_r(NULL, "^", &last);
  }
  free(range_ptr);

add_member:
  LL_Add(ll_setlist, strdup (buffer));
}

char *rpsl_macro_expand_add (char *range, char *name, irr_connection_t *irr, 
                             char *dbname) {
  irr_database_t *database;
  char buffer[BUFSIZE];
 
  strcpy (buffer, range);
  strcat (buffer, ",");
  strcat (buffer, name);
  convert_toupper (buffer); /* canonicalize the key for hash lookups */

  if (dbname != NULL) {
    strcat (buffer, ",");
    strcat (buffer, dbname);
  }

  LL_ContIterate (irr->ll_database, database) {
    if (dbname == NULL || strcasecmp (database->name, dbname)) {
      strcat (buffer, ",");
      strcat (buffer, database->name);
    }
  }

  return (strdup (buffer));
}

int chk_set_name (char *name) {

  if (name == NULL)
    return 0;

  if (!strncasecmp (name, "as-", 3)   ||
      !strncasecmp (name, "rs-", 3)   ||
      !strncasecmp (name, "rtrs-", 5) ||
      !strncasecmp (name, "fltr-", 5) ||
      !strncasecmp (name, "prng-", 5) ||
      strchr (name, ':'))
    return 1;

  return 0;
}
