/* 
 * $Id: route.c,v 1.5 2002/10/17 20:02:31 ljb Exp $
 * originally Id: route.c,v 1.9 1998/05/28 21:25:31 dogcow Exp 
 */

/* routines for dealing with prefixes based objects */

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

/* add_irr_prefix
 * Add a prefix to the radix tree
 */
void add_irr_prefix (irr_database_t *database, char *prefix_key, 
		    irr_object_t *object) {
  prefix_t *prefix;
  radix_tree_t *radix;
  radix_node_t *node;
  LINKED_LIST *ll_attr;
  irr_prefix_object_t tmp, *prefix_object;

  prefix = ascii2prefix (0, prefix_key);
  if (prefix == NULL)
    return;

  /* 
   * Store in memory
   */

  /* trace (NORM, default_trace, "radix_lookup(%s)\n", prefix_key); */
  if (prefix->family == AF_INET6)
    radix = database->radix_v6;
  else
    radix = database->radix_v4;
  node = radix_lookup (radix, prefix);

  if (node->data == NULL) {
    node->data = (void *) LL_Create (LL_Intrusive, True, 
				     LL_NextOffset, LL_Offset (&tmp, &tmp.next),
				     LL_PrevOffset, LL_Offset (&tmp, &tmp.prev),
				     LL_DestroyFunction, Delete_RT_Object, 
				     0);
  }
  Deref_Prefix (prefix);

  ll_attr = (LINKED_LIST *) node->data;

  prefix_object = New (irr_prefix_object_t);
  prefix_object->offset    = object->offset;
  prefix_object->len       = object->len;
  prefix_object->origin    = object->origin;
  prefix_object->type      = object->type;
  prefix_object->withdrawn = object->withdrawn;
  LL_Add (ll_attr, prefix_object);
}

/* delete_irr_prefix
 */
int delete_irr_prefix (irr_database_t *database, char *key, irr_object_t *object) {
  prefix_t *prefix = NULL;
  radix_tree_t *radix;
  radix_node_t *node = NULL;
  LINKED_LIST *ll_attr = NULL;
  irr_prefix_object_t *tmp_attr = NULL;
  int found = 0;

  prefix = ascii2prefix (0, key);
  if (prefix == NULL)
    return(found);

  if (prefix->family == AF_INET6)
    radix = database->radix_v6;
  else
    radix = database->radix_v4;

  node = prefix_search_exact (database, prefix);

  if (node != NULL) {
    ll_attr = (LINKED_LIST *) node->data;
    LL_Iterate (ll_attr, tmp_attr) {
      if (object->type == tmp_attr->type && object->origin == tmp_attr->origin) {
	found = 1;
	LL_Remove (ll_attr, tmp_attr); /* this is probably dangerous */
	break;
      }
    }
    if (LL_GetCount(ll_attr) == 0) {
      LL_Destroy (ll_attr);
      node->data = NULL;
      radix_remove (radix, node);
    }
  }
  Delete_Prefix (prefix);
  return (found);
}

/* seek_prefix_object
 * We need to find an object before we can delete it */
int seek_prefix_object (irr_database_t *database, enum IRR_OBJECTS type,
	 char *key, u_short origin, u_long *offset, u_long *len, int ignore_wd) {
  radix_node_t *node = NULL;
  LINKED_LIST *ll_attr;
  irr_prefix_object_t *prefix_object;
  prefix_t *prefix;

  if ((prefix = ascii2prefix (0, key)) == NULL)
     return -1;
  
  node = prefix_search_exact (database, prefix);
  Delete_Prefix (prefix);

  if (node == NULL) return (-1);

  ll_attr = (LINKED_LIST *) node->data;
  LL_Iterate (ll_attr, prefix_object) {
    if ( prefix_object->type   == type &&
         prefix_object->origin == origin &&
	(ignore_wd                  ||
	 prefix_object->withdrawn == 0)) {
      *offset = prefix_object->offset;
      *len = prefix_object->len;
      return (1);
    }
  }
  
  return (-1);
}

/* prefix_search_exact
 * Do an exact search (same bit length) for a prefix
 */
radix_node_t *prefix_search_exact (irr_database_t *database, prefix_t *prefix) {
  radix_tree_t *radix;

  if (prefix->family == AF_INET6)
    radix = database->radix_v6;
  else
    radix = database->radix_v4;

  return (radix_search_exact (radix, prefix));
}

/* prefix_search_best
 * Do an best search (same bit length, or smaller) for a prefix
 */
radix_node_t *prefix_search_best (irr_database_t *database, prefix_t *prefix) {
  radix_tree_t *radix;

  if (prefix->family == AF_INET6)
    radix = database->radix_v6;
  else
    radix = database->radix_v4;
  
  return (radix_search_best (radix, prefix, 0));
}
