/* $Id: llstackaux.c,v 1.1.1.1 2000/02/29 22:28:30 labovit Exp $
 * originally Id: llstackaux.c,v 1.1 1998/04/17 12:18:31 dogcow Exp  */

/* This file gets around certain complaints of the DEC cc that
   Push is a not allowable statement, for some reason. The macro
   is now a routine. */

#include "stack.h"
#include "linked_list.h"

/*extern void STACK_Push _STACK_P((STACK*, STACK_TYPE));*/

void myPush(STACK *s, STACK_TYPE d) {
	if (s->top != s->size) {
		s->array[s->top++] = (STACK_TYPE) d;
	} else {
		STACK_Push(s, (STACK_TYPE) d);
	}
}

#if 0
DATA_PTR myLL_GetHead(LINKED_LIST *ll) {
	if (ll->attr & LL_Intrusive) {
		return ll->head.data;
	} else {
		if ((ll->last = ll->head.cont) != NULL) {
			return ll->head.cont->data;
		} else {
			return (void *)NULL;
		}
	}
}

DATA_PTR myLL_GetPrev(LINKED_LIST *ll, DATA_PTR d) {
	if (ll->attr & LL_Intrusive) {
		return((DATA_PTR) ((char *) d + ll->prev_offset));
/* why that wacky casting, you may ask? because pointer arithmetic with
   void pointers is undefined, so it must be cast to somethint non-void;
   and char * is the pointer type with the least restrictions as far as
   alignment go. */
	} else {
		if ((ll->last) && (ll->last->data == d) &&
		   ((ll->last = ll->last->prev) != (void *) NULL)) {
			return ll->last->data;
		} else {
			return LList_GetPrev(ll, d);
		}
	}
}
   
#endif

