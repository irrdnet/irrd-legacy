/*
 * $Id: radix.h,v 1.2 2001/06/04 18:58:47 ljb Exp $
 */

#ifndef _RADIX_H
#define _RADIX_H

typedef struct _radix_node_t {
   u_int bit;			/* flag if this node used */
   prefix_t *prefix;		/* who we are in radix tree */
   struct _radix_node_t *l, *r;	/* left and right children */
   struct _radix_node_t *parent;/* may be used */
   void *data;			/* pointer to data */
} radix_node_t;

typedef struct _radix_tree_t {
   radix_node_t 	*head;
   u_int		maxbits; /* for 32 for IPv4, 128 for IPv6 */
   int num_active_node;		/* for debug purpose */
} radix_tree_t;

radix_node_t *radix_search_exact (radix_tree_t *radix, prefix_t *prefix);
radix_node_t *radix_search_exact_raw (radix_tree_t *radix, prefix_t *prefix);
//radix_node_t *radix_search_best (radix_tree_t *radix, prefix_t *prefix);
radix_node_t * radix_search_best (radix_tree_t *radix, prefix_t *prefix, 
				   int inclusive);
radix_node_t *radix_lookup (radix_tree_t *radix, prefix_t *prefix);
void radix_remove (radix_tree_t *radix, radix_node_t *node);
radix_tree_t *New_Radix (int maxbits);
void Clear_Radix (radix_tree_t *radix, void_fn_t func);
void Destroy_Radix (radix_tree_t *radix, void_fn_t func);
void radix_process (radix_tree_t *radix, void_fn_t func);

#define RADIX_MAXBITS 128   /* upto 128 bits (for IPv6) */
#define RADIX_NBIT(x)        (0x80 >> ((x) & 0x7f))
#define RADIX_NBYTE(x)       ((x) >> 3)

#define RADIX_DATA_GET(node, type) (type *)((node)->data)
#define RADIX_DATA_SET(node, value) ((node)->data = (void *)(value))

#define RADIX_WALK(Xhead, Xnode) \
    do { \
        radix_node_t *Xstack[RADIX_MAXBITS+1]; \
        radix_node_t **Xsp = Xstack; \
        radix_node_t *Xrn = (Xhead); \
        while ((Xnode = Xrn)) { \
            if (Xnode->prefix)

#define RADIX_WALK_ALL(Xhead, Xnode) \
do { \
        radix_node_t *Xstack[RADIX_MAXBITS+1]; \
        radix_node_t **Xsp = Xstack; \
        radix_node_t *Xrn = (Xhead); \
        while ((Xnode = Xrn)) { \
	    if (1)

#define RADIX_WALK_BREAK { \
	    if (Xsp != Xstack) { \
		Xrn = *(--Xsp); \
	     } else { \
		Xrn = (radix_node_t *) 0; \
	    } \
	    continue; }

#define RADIX_WALK_END \
            if (Xrn->l) { \
                if (Xrn->r) { \
                    *Xsp++ = Xrn->r; \
                } \
                Xrn = Xrn->l; \
            } else if (Xrn->r) { \
                Xrn = Xrn->r; \
            } else if (Xsp != Xstack) { \
                Xrn = *(--Xsp); \
            } else { \
                Xrn = (radix_node_t *) 0; \
            } \
        } \
    } while (0)

#endif /* _RADIX_H */
