/* 
 * $Id: route.c,v 1.3 2002/02/04 20:53:56 ljb Exp $
 * originally Id: route.c,v 1.9 1998/05/28 21:25:31 dogcow Exp 
 */

/* routines for dealing with route_objects */

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

/* add_irr_route
 * Add a route to disk special indicies (as a bit string like 111011101),
 * or store in memory radix tree
 */
void add_irr_route (irr_database_t *database, char *prefix_key, 
		    irr_object_t *object) {
  prefix_t *prefix;
  radix_node_t *node;
  LINKED_LIST *ll_attr;
  irr_route_object_t tmp, *rt_object;

  prefix = ascii2prefix (AF_INET, prefix_key);

  /* 
   * Store in memory
   */

  /* trace (NORM, default_trace, "radix_lookup(%s)\n", prefix_key);*/
  node = radix_lookup (database->radix, prefix);
  /*node = radix_lookup (database->radix, prefix);*/

  if (node->data == NULL) {
    node->data = (void *) LL_Create (LL_Intrusive, True, 
				     LL_NextOffset, LL_Offset (&tmp, &tmp.next),
				     LL_PrevOffset, LL_Offset (&tmp, &tmp.prev),
				     LL_DestroyFunction, Delete_RT_Object, 
				     0);
  }
  Deref_Prefix (prefix);

  ll_attr = (LINKED_LIST *) node->data;

  rt_object = New (irr_route_object_t);
  rt_object->offset    = object->offset;
  rt_object->len       = object->len;
  rt_object->origin    = object->origin;
  rt_object->withdrawn = object->withdrawn;
  LL_Add (ll_attr, rt_object);
}

/* delete_irr_route_radix
 */
int delete_irr_route (irr_database_t *database, char *key, irr_object_t *object) {
  prefix_t *prefix = NULL;
  radix_node_t *node = NULL;
  LINKED_LIST *ll_attr = NULL;
  irr_route_object_t *tmp_attr = NULL;
  int found = 0;

  prefix = ascii2prefix (AF_INET, key);

  node = route_search_exact (database, database->radix, prefix);

  if (node != NULL) {
    ll_attr = (LINKED_LIST *) node->data;
    LL_Iterate (ll_attr, tmp_attr) {
      if (object->origin == tmp_attr->origin) {
	found = 1;
	LL_Remove (ll_attr, tmp_attr); /* this is probably dangerous */
	break;
      }
    }
    if (LL_GetCount(ll_attr) == 0) {
      LL_Destroy (ll_attr);
      node->data = NULL;
      radix_remove (database->radix, node);
    }
  }
  Delete_Prefix (prefix);
  return (found);
}

/* find_route_object
 * We need to find an object before we can delete it */
int seek_route_object (irr_database_t *database, char *key, 
		       u_short origin, u_long *offset, u_long *len, 
		       int ignore_wd) {
  radix_node_t *node = NULL;
  LINKED_LIST *ll_attr;
  irr_route_object_t *rt_object;
  prefix_t *prefix;

  if ((prefix = ascii2prefix (AF_INET, key)) == NULL)
     return -1;
  
  node = radix_search_exact (database->radix, prefix);
  Delete_Prefix (prefix);

  if (node == NULL) return (-1);

  ll_attr = (LINKED_LIST *) node->data;
  LL_Iterate (ll_attr, rt_object) {
    if (rt_object->origin == origin &&
	(ignore_wd                  ||
	 rt_object->withdrawn == 0)) {
      *offset = rt_object->offset;
      *len = rt_object->len;
      return (1);
    }
  }
  
  return (-1);
}

/* route_search_exact
 * Do an exact search (same bit length) for a prefix
 * Either call the in memory radix routines, or go through
 * horrible contortions to pretend we have it on disk
 */
radix_node_t *route_search_exact (irr_database_t *database,
				  radix_tree_t *radix, prefix_t *prefix) {

  return (radix_search_exact (radix, prefix));
}

/* route_search_best
 * Do an best search (same bit length, or smaller) for a prefix
 * Either call the in memory radix routines, or go through
 * horrible contortions to pretend we have it on disk
 */
radix_node_t *route_search_best (irr_database_t *database,
				radix_tree_t *radix, prefix_t *prefix) {
  
  return (radix_search_best (radix, prefix, 0));
}
