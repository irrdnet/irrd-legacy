/* 
 * $Id: route.c,v 1.2 2001/07/13 19:15:41 ljb Exp $
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

radix_node_t *util_create_node (hash_spec_t *hspec);

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
  char *binarystring, data[BUFSIZE];

  prefix = ascii2prefix (AF_INET, prefix_key);

  /* 
   * Store binary version of route in gdbm file 
   */

  if (IRR.use_disk == 1) { 
    binarystring = prefix_tobitstring (prefix);
    
    /* origin withdrawin offset len */
    sprintf (data, "%d#%d#%d#%d", object->origin, object->withdrawn,
             (int) object->offset, (int) object->len);

    memory_hash_spec_store (database, binarystring, ROUTE_BITSTRING,
                            NULL, NULL, data);

    Delete (binarystring);
    Delete_Prefix (prefix);
    return;
  }

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
  char blob[40], *bitstring = NULL;
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
    if ((LL_GetCount ( ll_attr) == 0) && (IRR.use_disk == 0)) {
      LL_Destroy (ll_attr);
      node->data = NULL;
      radix_remove (database->radix, node);
    }
  }

  /* make change to disk */
  if (IRR.use_disk == 1) {
    bitstring = prefix_tobitstring (prefix);
    sprintf (blob, "%d#", object->origin);
    memory_hash_spec_remove (database, bitstring, ROUTE_BITSTRING, blob);
    /* more memory cleanup here -- lets see what PURIFY says... */
    Delete (node);
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
  char *bitstring;
  hash_spec_t *hspec = NULL;

  if ((prefix = ascii2prefix (AF_INET, key)) == NULL)
     return -1;
  
  if (IRR.use_disk == 0) 
    node = radix_search_exact (database->radix, prefix);
  else {
    bitstring = prefix_tobitstring (prefix);
    hspec = fetch_hash_spec (database, bitstring, UNPACK);
    Delete (bitstring);
    if (hspec) {
      node = util_create_node (hspec);
      Delete_hash_spec (hspec);
    }
  }
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
  char *bitstring;
  hash_spec_t *hspec = NULL;
  radix_node_t *node;

  /* 
   * Not using disk -- use memory 
   */
  if (IRR.use_disk == 0) 
    return (radix_search_exact (radix, prefix));
  /* ----- end memory ----- */

  bitstring = prefix_tobitstring (prefix);
  hspec = fetch_hash_spec (database, bitstring, UNPACK);
  Delete (bitstring);

  /* found something */
  if (hspec) {
    node = util_create_node (hspec);
    Delete_hash_spec (hspec);
    return (node);
  }

  return (NULL);
}


/* route_search_best
 * Do an best search (same bit length, or smaller) for a prefix
 * Either call the in memory radix routines, or go through
 * horrible contortions to pretend we have it on disk
 */
radix_node_t *route_search_best (irr_database_t *database,
				radix_tree_t *radix, prefix_t *prefix) {
  char *bitstring;
  int bitpos;
  hash_spec_t *hspec = NULL;
  radix_node_t *node;
  
  /* 
   * Not using disk -- use memory 
   */
  if (IRR.use_disk == 0) 
    return (radix_search_best (radix, prefix, 0));
  /* ----- end memory ----- */

  bitstring = prefix_tobitstring (prefix);
  bitpos = prefix->bitlen;

  while (bitpos-- > 0) {
    hspec = fetch_hash_spec (database, bitstring, UNPACK); 

    if (hspec != NULL) 
      break;

    /*printf ("%s\n", bitstring);*/
    bitstring[bitpos] = '-';
  }
  
  Delete (bitstring);

  /* found something */
  if (hspec) {
    node = util_create_node (hspec);
    node->prefix = copy_prefix (prefix);
    node->prefix->bitlen = bitpos;
    Delete_hash_spec (hspec);
    return (node);
  }
  
  return (NULL);
}




/* Route searches. M - all more specific eg, !r199.208.0.0/16,M */
/* Also does m - one level only more specific */
void route_search_more_specific (irr_connection_t *irr, 
				 irr_database_t *database, prefix_t *prefix) {
  char *bitstring;
  hash_spec_t *hspec;
  int bitpos;
  irr_hash_string_t *hash_string;
  irr_route_object_t *rt_object;


  bitpos = prefix->bitlen;
  bitstring = prefix_tobitstring (prefix);

  printf ("%s\n", bitstring);
  hspec = fetch_hash_spec (database, bitstring, UNPACK); 

  if (hspec != NULL) {
    LL_Iterate (hspec->ll_1, hash_string) {
      rt_object = New (irr_route_object_t);
    
      sscanf (hash_string->string, "%d#%d#%ld#%ld",
	      (int *) &(rt_object->origin), (int *) &(rt_object->withdrawn),
	      &(rt_object->offset), &(rt_object->len));

      if (irr->withdrawn == 1 || rt_object->withdrawn == 0) {
	if (irr->full_obj == 0)
	  irr_build_key_answer (irr, database->fd, database->name, ROUTE, 
				rt_object->offset, rt_object->origin);
	else
	  irr_build_answer (irr, database->fd, ROUTE, rt_object->offset, 
			    rt_object->len, NULL, database->db_syntax);
      }
    }
  }
}


/* util_create_node
 * Fake a radix node so the irr_less and irr_more routines
 * don't know that we really got the answer from disk
 * A bit of a hack, but it was easier then duplicating all of 
 * the irr_less and irr_more logic.
 * We just need to remember to delete these fake radix
 * nodes when we're done with the irr_more/less routines
 */
radix_node_t *util_create_node (hash_spec_t *hspec) {
  radix_node_t *node;
  LINKED_LIST *ll_list;
  irr_hash_string_t *hash_string;
  int origin, withdrawn, offset, len;
  irr_route_object_t *rt_object;

  node = New (radix_node_t);
  ll_list = LL_Create (LL_DestroyFunction, Delete_RT_Object, NULL);

  LL_Iterate (hspec->ll_1, hash_string) {
    rt_object = New (irr_route_object_t);
    
    sscanf (hash_string->string, "%d#%d#%d#%d",
	    &origin, &withdrawn, &offset, &len);

    rt_object->origin = origin;
    rt_object->withdrawn = withdrawn;
    rt_object->offset = offset;
    rt_object->len = len;

    LL_Add (ll_list, rt_object);
  }

  node->data = ll_list;
  return (node);
}



