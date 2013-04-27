/* 
 * $Id: route.c,v 1.5 2002/10/17 20:02:31 ljb Exp $
 * originally Id: route.c,v 1.9 1998/05/28 21:25:31 dogcow Exp 
 */

/* routines for dealing with prefixes based objects */

#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <signal.h>
#include <fcntl.h>

#include "mrt.h"
#include "trace.h"
#include "config_file.h"
#include "irrd.h"

/* add_irr_prefix
 * Add a prefix to the radix tree
 */
void add_irr_prefix (irr_database_t *database, prefix_t *prefix, 
		    irr_object_t *object) {
  radix_tree_t *radix;
  radix_node_t *node;
  irr_prefix_object_t *prefix_object;

  /* 
   * Store in radix tree
   */

  if (prefix->family == AF_INET6)
    radix = database->radix_v6;
  else
    radix = database->radix_v4;

  /* see if node already exists, and if not, create it */
  node = radix_lookup (radix, prefix);
  Deref_Prefix (prefix);

  prefix_object = irrd_malloc(sizeof(irr_prefix_object_t));
  prefix_object->next    = NULL;
  prefix_object->offset  = object->offset;
  prefix_object->len     = object->len;
  prefix_object->origin  = object->origin;
  prefix_object->type    = object->type;
  if (node->data != NULL) {
    prefix_object->next = (irr_prefix_object_t *) node->data;
  }
  node->data = prefix_object;
}

/* delete_irr_prefix
 */
int delete_irr_prefix (irr_database_t *database, prefix_t *prefix, irr_object_t *object) {
  radix_tree_t *radix;
  radix_node_t *node = NULL;
  irr_prefix_object_t *prefix_object, *last_prefix_obj = NULL;
  int found = 0;

  if (prefix->family == AF_INET6)
    radix = database->radix_v6;
  else
    radix = database->radix_v4;

  node = prefix_search_exact (database, prefix);
  Deref_Prefix (prefix);

  if (node != NULL) {
    prefix_object = node->data;
    while (prefix_object != NULL) {
      if (object->type == prefix_object->type && object->origin == prefix_object->origin && object->offset == prefix_object->offset) {
	found = 1;
	if (last_prefix_obj != NULL)
	  last_prefix_obj->next = prefix_object->next;
	else
	  node->data = prefix_object->next;
	free(prefix_object);
	break;
      }
      last_prefix_obj = prefix_object;
      prefix_object = prefix_object->next;
    }
    if (node->data == NULL) {
      radix_remove (radix, node);
    }
  }
  return (found);
}

/* seek_prefix_object
 * We need to find an object before we can delete it */
int seek_prefix_object (irr_database_t *database, enum IRR_OBJECTS type,
	 char *key, uint32_t origin, u_long *offset, u_long *len) {
  radix_node_t *node = NULL;
  irr_prefix_object_t *prefix_object;
  prefix_t *prefix;
  int family = AF_INET;

  if (type != ROUTE)
    family = AF_INET6;

  if ((prefix = ascii2prefix (family, key)) == NULL)
     return -1;
  
  node = prefix_search_exact (database, prefix);
  Deref_Prefix (prefix);

  if (node == NULL) return (-1);

  prefix_object = (irr_prefix_object_t *) node->data;
  while (prefix_object != NULL) {
    if ( prefix_object->type   == type &&
         prefix_object->origin == origin ) {
      *offset = prefix_object->offset;
      *len = prefix_object->len;
      return (1);
    }
    prefix_object = prefix_object->next;
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
