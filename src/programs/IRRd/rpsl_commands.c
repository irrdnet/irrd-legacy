/*
 * $Id: rpsl_commands.c,v 1.8 2002/02/04 20:53:56 ljb Exp $
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
#include "radix.h"
#include "hash.h"
#include "stack.h"
#include <fcntl.h>
#include "irrd.h"

extern trace_t *default_trace;


/* local routines */
void update_members_list (char *range_op, int expand_flag, 
			  LINKED_LIST *ll_member_examined, 
			  LINKED_LIST *ll_aslist, LINKED_LIST *ll_set_names, 
			  STACK *stack, char *db, irr_connection_t *irr);
void search_autnum_members_of (irr_database_t *database, enum SPEC_KEYS id,
			       LINKED_LIST *ll_mbrs_ref, char *range_op, char *set_name, 
			       LINKED_LIST *ll_aslist);
void mbrs_by_ref_as_set (irr_database_t *database, enum SPEC_KEYS id,
			 char *range_op, int expand_flag, 
			 LINKED_LIST *ll_aslist, char *set_name,
			 LINKED_LIST *ll_mbr_by_ref);
char *rpsl_macro_expand_add (char *range_op, char *name, irr_connection_t *irr, 
                             char *dbname);
void SL_Add (LINKED_LIST *ll_aslist, char *p, char *range_op);
int set_name (char *);

/* as-set expansion !ias-bar */
void irr_as_set_expand (irr_connection_t *irr, char *name) {
  irr_database_t *database;
  STACK *stack;
  LINKED_LIST *ll_member_examined, *ll_aslist;
  char *as, *as_set_name, *mstr, *db;
  char *lasts, *range_op, key[BUFSIZE];
  int first, id, expand_flag = 0;
  hash_spec_t *hash_item;

  if (strchr (name, ',') != NULL) {
    strtok_r (name, ",", &lasts);
    expand_flag = 1;
  }
  id = SET_MBRSX; /* this identifies the type of hash entry to use */

  convert_toupper (name);
  stack = Create_STACK (500);
  ll_member_examined = LL_Create (LL_DestroyFunction, free, NULL);
  ll_aslist = LL_Create (LL_AutoSort, True, LL_CompareFunction, irr_comp, 
			 LL_DestroyFunction, free, NULL);
  mstr = rpsl_macro_expand_add (NULL, name, irr, NULL);
  Push (stack, mstr);
  LL_Add (ll_member_examined, strdup (name));

  while (Get_STACK_Count (stack) > 0) {
    mstr = (char *) Pop (stack);
    /* might want to check the examined list to see if this set name
       has been examined already */
    first = 1;
    lasts = NULL;
    range_op = strtok_r (mstr, ",", &lasts);
    if (!strcmp (range_op, " ")) range_op = NULL;
    as_set_name = strtok_r (NULL, ",", &lasts);

    while ((db = strtok_r (NULL, ",", &lasts)) != NULL) {
      if ((database = find_database (db)) == NULL) {
        trace (NORM, default_trace, "ERROR -- irr_as_set_expand(): Database not found %s\n", db);
        break;
      }
      irr_lock (database);
 /*     if ((hash_item = HASH_Lookup (database->hash_spec, as_set_name)) != NULL) {*/
      make_setobj_key (key, as_set_name);
      if ((hash_item = fetch_hash_spec (database, key, UNPACK)) != NULL) {

          first = 0; 
	  update_members_list (range_op, expand_flag, ll_member_examined, 
			       ll_aslist, hash_item->ll_1, stack, 
			       db, irr);
	  mbrs_by_ref_as_set (database, id, range_op, expand_flag, ll_aslist, 
                              as_set_name, hash_item->ll_2);
	  Delete_hash_spec (hash_item); 
      }
      irr_unlock (database);
      if (first == 0)
        break;
    }
    free (mstr);
  }

  first = 1;
  LL_ContIterate (ll_aslist, as) {
    if (!first) irr_add_answer (irr, " ");
    irr_add_answer (irr, "%s", as);
    first = 0;
  }
  irr_send_answer (irr);
  LL_Destroy (ll_aslist);
  LL_Destroy (ll_member_examined);
  Destroy_STACK (stack);
}

void mbrs_by_ref_as_set (irr_database_t *database, enum SPEC_KEYS id,
			 char *range_op, int expand_flag, 
			 LINKED_LIST *ll_aslist, char *set_name,
			 LINKED_LIST *ll_mbr_by_ref) {
  char *member;

  if (ll_mbr_by_ref == NULL) 
    return;

  if (expand_flag == 0) {
    LL_ContIterate (ll_mbr_by_ref, member) {
      LL_Add(ll_aslist, strdup (member));
    }
    return;
  }

  search_autnum_members_of (database, id, ll_mbr_by_ref, range_op, 
			    set_name, ll_aslist);   
}

/* Find all the route or autnum's which reference the set name 
 * (via 'members-of:').
 */
void search_autnum_members_of (irr_database_t *database, enum SPEC_KEYS id,
			       LINKED_LIST *ll_mbr_by_ref, char *range_op, 
                               char *set_name, LINKED_LIST *ll_aslist) {
  char *p, *maint, key[BUFSIZE];
  hash_spec_t *hash_item;
  
  LL_ContIterate (ll_mbr_by_ref, maint) {
    make_spec_key (key, maint, set_name);
/*
    if ((hash_item = HASH_Lookup (database->hash_spec, key)) != NULL) {
*/
    if ((hash_item = fetch_hash_spec (database, key, UNPACK)) != NULL) {
      LL_ContIterate (hash_item->ll_1, p) {
	SL_Add (ll_aslist, p, range_op);
      }
      Delete_hash_spec (hash_item);
    }
  }

}

void update_members_list (char *range_op, int expand_flag, 
			  LINKED_LIST *ll_member_examined, 
			  LINKED_LIST *ll_aslist, LINKED_LIST *ll_set_names, 
			  STACK *stack, char *db, irr_connection_t *irr) {
  char *as, *p, *q, *r;
  char buffer[BUFSIZE];
  int already_examined;

  if (ll_set_names == NULL)
    return;

  LL_ContIterate (ll_set_names, as) {
    if ((expand_flag == 0) || !set_name (as)) {
      SL_Add (ll_aslist, as, range_op);
    }
    /* orig
    if ((expand_flag == 0) || (!((p = strchr (as, '-')) != NULL &&
				 ((p - as) == 2)))) {
      SL_Add (ll_aslist, as, range_op);
    }
    */
    else { /* we have a set name */
      already_examined = 0;
      LL_ContIterate (ll_member_examined, p) {
	if (!strcmp (p, as)) {
	  already_examined = 1;
	  break;
	}
      }
      if (!already_examined) {
	q = range_op;
	r = as;
	/* Need to seperate the range op from the set 
	   name for rpsl_macro_expand_add() */
	if ((p = strchr (as, '^')) != NULL) {
	  strncpy (buffer, as, p - as);
	  buffer[p - as] = '\0';
	  q = p;      /* this is the range op */ 
	  r = buffer; /* this is the set name without the range op */
        } 
        p = rpsl_macro_expand_add (q, r, irr, db); 
        Push (stack, p);
        LL_Add (ll_member_examined, strdup (r));
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
  if (*p++ != '^')
    return INVALID_RANGE; /* make sure this is really a range */
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
void SL_Add (LINKED_LIST *ll_aslist, char *p, char *range_op) {
  char buffer[BUFSIZE], rangestr[32];
  char *q, *r;
  unsigned int bitlen;
  int found = 0, n;
  enum  PREFIX_RANGE_TYPE range_op_type, prefix_range_type;
  unsigned int range_op_start, range_op_end, prefix_range_start, prefix_range_end;

  r = p;

  if (range_op != NULL) {
    range_op_type = prefix_range_parse(range_op, &range_op_start, &range_op_end);
    if (range_op_type == INVALID_RANGE) {
      trace (NORM, default_trace, "SL_Add(): range_op is invalid : %s\n", range_op);
      return;
    }
    /* should have a prefix length to be legal */
    if ( (q = strchr(p, '/')) != NULL )
      bitlen = atoi(q + 1);
    else {
      trace (NORM, default_trace, "SL_Add(): prefix missing: %s\n", p);
      return;
    }

    strcpy (buffer, p);
    if ( (q = strchr(buffer, '^')) == NULL ) /* check for range on prefix */
      prefix_range_start = (prefix_range_end = bitlen);
    else {
      prefix_range_type = prefix_range_parse(q, &prefix_range_start, &prefix_range_end);
      if (prefix_range_type == INVALID_RANGE) {
        trace (NORM, default_trace, "SL_Add(): prefix range is invalid : %s\n", buffer);
        return;
      }
      *q = 0;	/* terminate string at range */
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
	if (range_op_end < prefix_range_start)
	  return;  /* apply an less specific range to a more specific one */
	if (prefix_range_start > range_op_start)
	  range_op_start = prefix_range_start;
    }

    if (range_op_start > range_op_end)
	return; /* specific range exceeds maximum, toss prefix */
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
    r = buffer;
  }

  LL_ContIterate (ll_aslist, q) {
    if ((n = strcmp (r, q)) == 0) {
      found = 1;
      break;
    }
    else if (n < 0) /* list is in ascending order, no need to look further */
      break;
  }
  if (found == 0)
    LL_Add(ll_aslist, strdup (r));
}

char *rpsl_macro_expand_add (char *range_op, char *name, irr_connection_t *irr, 
                             char *dbname) {
  irr_database_t *database;
  char buffer[BUFSIZE];
 
  if (range_op != NULL)
    strcpy (buffer, range_op);
  else
    strcpy (buffer, " ");

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

int set_name (char *name) {

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
