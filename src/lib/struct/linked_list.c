/*-----------------------------------------------------------
 *  Name: 	linked_list.c
 *  Created:	Fri Apr 22 02:31:08 1994
 *  Author: 	Jonathan DeKock   <dekock@winter>
 *  DESCR:  	generic linked list code
 */

#include <stdarg.h>
#include <linked_list.h>
#include <sys/types.h>
#include <defs.h>
#include <irrdmem.h>

/******************* Global Variables **********************/
static LL_SortProc    LL_SortFunction   = NULL;

/******************* Internal Macros ***********************/
#define tfm(a) ((a) ? tfs[1] : tfs[0])  /* Returns "True" or "False" */

/* Intrusive Macros for Next and Prev */
#define NextP(ll, d) *(DATA_PTR*)((char*)d + ll->next_offset)
#define PrevP(ll, d) *(DATA_PTR*)((char*)d + ll->prev_offset)

/*-----------------------------------------------------------
 *  Name(s): 	LLM_PS()
 *              LLM_PD()
 *              LLM_PH()
 *              LLM_PO()
 *  Created:	Wed Aug 31 00:25:50 1994
 *  Author: 	Jonathan DeKock   <dekock@winter>
 *  DESCR:  	prints the attribute and its value.
 */
#define LLM_PS(ll, attr, val) printf("LINKED LIST: 0x%.8x attribute %-18s = %s\n", (u_int)ll, attr, val)
#define LLM_PD(ll, attr, val) printf("LINKED LIST: 0x%.8x attribute %-18s = %d\n", (u_int)ll, attr, (u_int)val)
#define LLM_PH(ll, attr, val) printf("LINKED LIST: 0x%.8x attribute %-18s = 0x%.8x\n", (u_int)ll, attr, (u_int)val)

/*-----------------------------------------------------------
 *  Name: 	LLMacro_PrintAllAttr()
 *  Created:	Wed Aug 31 00:26:42 1994
 *  Author: 	Jonathan DeKock   <dekock@winter>
 *  DESCR:  	prints all the attributes
 */
#define LLMacro_PrintAllAttr(ll)\
{\
   LLM_PS(ll, "LL_Intrusive",     tfm(ll->attr & LL_Intrusive));\
   LLM_PS(ll, "LL_AutoSort",      tfm(ll->attr & LL_AutoSort));\
   LLM_PS(ll, "LL_ReportChange",  tfm(ll->attr & LL_ReportChange));\
   LLM_PS(ll, "LL_ReportAccess",  tfm(ll->attr & LL_ReportAccess));\
   LLM_PD(ll, "ll->count",        ll->count);\
   if (ll->attr & LL_Intrusive) {\
      LLM_PD(ll, "LL_NextOffset", ll->next_offset);\
      LLM_PD(ll, "LL_PrevOffset", ll->prev_offset);\
   }\
   LLM_PH(ll, "LL_FindFunction",    ll->find_fn);\
   LLM_PH(ll, "LL_CompareFunction", ll->comp_fn);\
   LLM_PH(ll, "LL_DestroyFunction", ll->destroy_fn);\
}


/*-----------------------------------------------------------
 *  Name: 	LLMacro_SetAttr()
 *  Created:	Wed Aug 31 00:44:58 1994
 *  Author: 	Jonathan DeKock   <dekock@winter>
 *  DESCR:  	sets the attribute attr with the next arg.
 */
#define LLMacro_SetAttr(ll, attr, ap, val, ptr) \
{ \
   switch (attr) {\
    case LL_Intrusive:\
    case LL_AutoSort:\
    case LL_ReportChange:\
    case LL_ReportAccess:\
      val = va_arg(ap, int);\
      if (val) ll->attr = (enum LL_ATTR)(ll->attr | attr);\
      else     ll->attr = (enum LL_ATTR)(ll->attr & ~attr);\
      break;\
    case LL_NonIntrusive:\
      val = va_arg(ap, int);\
      if (val) ll->attr = (enum LL_ATTR)(ll->attr & ~LL_Intrusive);\
      else     ll->attr = (enum LL_ATTR)(ll->attr |  LL_Intrusive);\
      break;\
    case LL_NextOffset:\
      val = va_arg(ap, int);\
      ll->next_offset = (short) val;\
      ll->attr = (enum LL_ATTR)(ll->attr | LL_Intrusive);\
      break;\
    case LL_PrevOffset:\
      val = va_arg(ap, int);\
      ll->prev_offset = (short) val;\
      ll->attr = (enum LL_ATTR)(ll->attr | LL_Intrusive);\
      break;\
    case LL_PointersOffset:\
      val = (enum LL_ATTR) va_arg(ap, int);\
      ll->next_offset = (short) val;\
      ll->prev_offset = (short) val + sizeof(DATA_PTR);\
      ll->attr = (enum LL_ATTR)(ll->attr | LL_Intrusive);\
      break;\
    case LL_FindFunction:\
      ptr = va_arg(ap, DATA_PTR);\
      ll->find_fn = (LL_FindProc)ptr;\
      break;\
    case LL_CompareFunction:\
      ptr = va_arg(ap, DATA_PTR);\
      ll->comp_fn = (LL_CompareProc)ptr;\
      break;\
    case LL_DestroyFunction:\
      ptr = va_arg(ap, DATA_PTR);\
      ll->destroy_fn = (LL_DestroyProc)ptr;\
      break;\
    default:\
       attr = (enum LL_ATTR)0;  /* inform caller of failure */\
       break;\
   }\
}

/*-----------------------------------------------------------
 *  Name: 	LLMacro_GetContainer()
 *  Created:	Fri Sep  2 02:10:48 1994
 *  Author: 	Jonathan DeKock   <dekock@winter>
 *  DESCR:  	gets a container for an item dddddd
 */
#define LLMacro_GetContainer(ll, result, input)\
{\
   if ((ll->last) && (ll->last->data == input)) {\
      result = ll->last;\
   }\
   else if ((ll->last) && (ll->last->next != NULL) && (ll->last->next->data == input)) {\
      result = ll->last->next;\
   }\
   else if ((ll->last) && (ll->last->prev) && (ll->last->prev->data == input)) {\
      result = ll->last->prev;\
   }\
   else {\
      for (result = ll->head.cont; result; result = result->next) {\
	 if (result->data == input) break;\
      }\
   }\
}

/*-----------------------------------------------------------
 *  Name: 	LLMacro_ContPlaceBetween()
 *  Created:	Fri Sep  2 17:29:37 1994
 *  Author: 	Jonathan DeKock   <dekock@winter>
 *  DESCR:  	places container "elt" between "before" and "after"
 */
#define LLMacro_ContPlaceBetween(ll, elt, before, after)\
{\
   elt->next = after;\
   elt->prev = before;\
   if (before) {\
      before->next = elt;\
      if (after) after->prev = elt;\
      else ll->tail.cont = elt;\
   }\
   else {\
      if (after) after->prev = elt;\
      else ll->tail.cont = elt;\
      ll->head.cont = elt;\
    }\
}

/*-----------------------------------------------------------
 *  Name: 	LLMacro_IntrPlaceBetween()
 *  Created:	Fri Sep  2 17:40:56 1994
 *  Author: 	Jonathan DeKock   <dekock@winter>
 *  DESCR:  	places data "elt" between "before" and "after"
 */
#define LLMacro_IntrPlaceBetween(ll, elt, before, after)\
{\
   NextP(ll, elt) = after;\
   PrevP(ll, elt) = before;\
   if (before) {\
      NextP(ll, before) = elt;\
      if (after) PrevP(ll, after) = elt;\
      else ll->tail.data = elt;\
   }\
   else {\
      if (after) PrevP(ll, after) = elt;\
      else ll->tail.data = elt;\
      ll->head.data = elt;\
   }\
}

/******************* Function Definitions ******************/

/*-----------------------------------------------------------
 *  Name: 	LL_Create()
 *  Created:	Tue Aug 30 19:12:42 1994
 *  Author: 	Jonathan DeKock   <dekock@winter>
 *  DESCR:  	Creates and initilizes a linked list with var. args
 */
LINKED_LIST *LL_Create(int first, ...)
{
   va_list         ap;
   enum LL_ATTR    attr;
   int             val;
   DATA_PTR        ptr;
   LINKED_LIST    *ll;

   /* Create it */
   if (!(ll = irrd_malloc(sizeof(LINKED_LIST)))) {
      return(NULL);
   }

   /* Initialize it */
   ll->head.data    = NULL;
   ll->head.cont    = NULL;
   ll->tail.data    = NULL;
   ll->tail.cont    = NULL;
   ll->last         = NULL;
   ll->find_fn      = NULL;
   ll->comp_fn      = NULL;
   ll->destroy_fn   = NULL;
   ll->count        = 0;
   ll->next_offset  = 0;
   ll->prev_offset  = sizeof(DATA_PTR);
   ll->attr         = (enum LL_ATTR) 0;

   /* Process the Arguments */
   va_start(ap, first);
   for (attr = (enum LL_ATTR)first; attr; attr = va_arg(ap, enum LL_ATTR)) {
      LLMacro_SetAttr(ll, attr, ap, val, ptr);
      if (!attr) break; /* something went wrong inside --> hit default, bad attribute*/
   }
   va_end(ap);

   return(ll);
}

/*-----------------------------------------------------------
 *  Name: 	LL_ClearFn()
 *  Created:	Wed Aug 31 01:31:06 1994
 *  Author: 	Jonathan DeKock   <dekock@winter>
 *  DESCR:  	removes every item from the list,
 *              but leaves attributes intact.
 */
void LL_ClearFn(LINKED_LIST *ll, LL_DestroyProc destroy)
{
   DATA_PTR data;

   while ((data = LL_GetHead(ll))) { 
      LL_RemoveFn(ll, data, destroy);
   }
}

/*-----------------------------------------------------------
 *  Name: 	LL_DestroyFn()
 *  Created:	Wed Aug 31 01:57:27 1994
 *  Author: 	Jonathan DeKock   <dekock@winter>
 *  DESCR:  	destroys a linked list and frees all Memory
 */
void LL_DestroyFn(LINKED_LIST *ll, LL_DestroyProc destroy)
{
   LL_ClearFn(ll, destroy);
   irrd_free(ll);
}

/*-----------------------------------------------------------
 *  Name: 	LL_SetAttributes()
 *  Created:	Wed Aug 31 03:29:08 1994
 *  Author: 	Jonathan DeKock   <dekock@winter>
 *  DESCR:  	sets 1 or more attributes
 */
void LL_SetAttributes(LINKED_LIST *ll, enum LL_ATTR first, ...)
{
   va_list      ap;
   enum LL_ATTR attr;
   int          val;
   DATA_PTR     ptr;
   
   va_start(ap, first);
   for (attr = first; attr; attr = va_arg(ap, enum LL_ATTR)) {
      LLMacro_SetAttr(ll, attr, ap, val, ptr);
      if (!attr) break; /* Attribute set failure --> BAD ATTRIBUTE  */
   }
   va_end(ap);
}

/*-----------------------------------------------------------
 *  Name: 	LL_Append()
 *  Created:	Wed Aug 31 16:49:07 1994
 *  Author: 	Jonathan DeKock   <dekock@winter>
 *  DESCR:  	appends a item onto the linked list
 */
DATA_PTR LL_Append(LINKED_LIST *ll, DATA_PTR data)
{
   if (ll->attr & LL_Intrusive) {
      LLMacro_IntrPlaceBetween(ll, data, ll->tail.data, NULL);
   } 
   else { 
      LL_CONTAINER *NULL_CONT = NULL; /* Required to make macro work */
      LL_CONTAINER *cont = irrd_malloc(sizeof(LL_CONTAINER));
      if (!cont) {
	 return(NULL);
      }
      cont->data = data;
      LLMacro_ContPlaceBetween(ll, cont, ll->tail.cont, NULL_CONT);
      ll->last = cont;
   }
   ll->count++;
   
   return(data);
}

/*-----------------------------------------------------------
 *  Name: 	LL_Prepend()
 *  Created:	Thu Sep  1 14:40:33 1994
 *  Author: 	Jonathan DeKock   <dekock@winter>
 *  DESCR:  	places data on the head of the list
 */
DATA_PTR LL_Prepend(LINKED_LIST *ll, DATA_PTR data)
{
   if (ll->attr & LL_Intrusive) {
      LLMacro_IntrPlaceBetween(ll, data, NULL, ll->head.data);
   }  
   else { 
      LL_CONTAINER *NULL_CONT = NULL; /* Required to make macro work */
      LL_CONTAINER *cont = irrd_malloc(sizeof(LL_CONTAINER));
      if (!cont) {
	 return(NULL);
      }
      cont->data = data;
      LLMacro_ContPlaceBetween(ll, cont, NULL_CONT, ll->head.cont);
      ll->last = cont;
   }
   ll->count++;
   
   return(data);
}

/*-----------------------------------------------------------
 *  Name: 	LL_InsertSorted()
 *  Created:	Thu Sep  1 23:44:55 1994
 *  Author: 	Jonathan DeKock   <dekock@winter>
 *  DESCR:  	inserts an element into a sorted list
 */
DATA_PTR LL_InsertSorted(LINKED_LIST *ll, DATA_PTR data, ...)
{
   LL_CompareProc compare = ll->comp_fn;
   
   if (!(compare)) {
      va_list ap;
      va_start(ap, data);
      compare = va_arg(ap, LL_CompareProc);
      va_end(ap);
   }

   LL_InsertSortedFn(ll, data, compare);

   return(data);
}

/*-----------------------------------------------------------
 *  Name: 	LL_InsertSortedFn()
 *  Created:	Thu Sep  1 23:49:05 1994
 *  Author: 	Jonathan DeKock   <dekock@winter>
 *  DESCR:  	inserts an element into a sorted list by function compare
 */
DATA_PTR LL_InsertSortedFn(LINKED_LIST *ll, DATA_PTR data, LL_CompareProc compare)
{
   DATA_PTR before, after;
   
   /* If the list is Intrusive */
   if (ll->attr & LL_Intrusive) {
      /* Search backwards to find position */
      for (after = NULL, before = ll->tail.data; before; after = before, before = PrevP(ll, before)) {
	 if ((compare)(before, data) <= 0) break;    /* Got it, insert between before and after */
      }
      LLMacro_IntrPlaceBetween(ll, data, before, after);
   }
   else { /* Container */
      LL_CONTAINER *before_cont, *after_cont, *cont;
      cont = irrd_malloc(sizeof(LL_CONTAINER));
      if (!cont) {
	 return(NULL);
      }
      /* Search backwards to find position */
      for (after_cont = NULL, before_cont = ll->tail.cont; before_cont;
	   after_cont = before_cont, before_cont = before_cont->prev) {
	 if ((compare)(before_cont->data, data) <= 0) break;  /* Got it, insert between before and after */
      }
      cont->data = data;
      LLMacro_ContPlaceBetween(ll, cont, before_cont, after_cont);
      before = (before_cont) ? before_cont->data : NULL;
      after  = (after_cont)  ? after_cont->data  : NULL;
      ll->last = cont;
   }
   ll->count++;
   
   return(data);
}

/*-----------------------------------------------------------
 *  Name: 	LL_InsertAfter()
 *  Created:	Fri Sep  2 01:51:07 1994
 *  Author: 	Jonathan DeKock   <dekock@winter>
 *  DESCR:  	inserts a data element after another elt.
 *              before is the item "before me" or
 *              the item which i am to follow
 *              i.e. i am the item after BEFORE
 */
DATA_PTR LL_InsertAfter(LINKED_LIST *ll, DATA_PTR data, DATA_PTR before)
{
   if (ll->attr & LL_Intrusive) {
      DATA_PTR after;
      if (before) {
	 after = NextP(ll, before);
      }
      else { /* if !before */
	 after = ll->head.data;
      }
      LLMacro_IntrPlaceBetween(ll, data, before, after);
   }
   else { /* Container */
      LL_CONTAINER *after_cont, *before_cont, *cont;
      cont = irrd_malloc(sizeof(LL_CONTAINER));
      if (!cont) {
	 return(NULL);
      }
      if (before) {
	 LLMacro_GetContainer(ll, before_cont, before);
	 after_cont = before_cont->next;
      }
      else {
	 after_cont = ll->head.cont;
	 before_cont = NULL;
      }
      cont->data = data;
      LLMacro_ContPlaceBetween(ll, cont, before_cont, after_cont);
      ll->last = cont;
   }
   ll->count++;

   return(data);
}

/*-----------------------------------------------------------
 *  Name: 	LL_InsertBefore()
 *  Created:	Fri Sep  2 03:14:46 1994
 *  Author: 	Jonathan DeKock   <dekock@winter>
 *  DESCR:  	inserts an element before another element
 *              after is the element which will be "after" me.
 */
DATA_PTR LL_InsertBefore(LINKED_LIST *ll, DATA_PTR data, DATA_PTR after)
{
   if (ll->attr & LL_Intrusive) {
      DATA_PTR before;
      if (after) {
	 before = PrevP(ll, after); 
      }
      else {
	 before = ll->tail.data;
      }
      LLMacro_IntrPlaceBetween(ll, data, before, after);
   }
   else { 
      LL_CONTAINER *before_cont, *after_cont, *cont;
      cont = irrd_malloc(sizeof(LL_CONTAINER));
      if (!cont) {
	 return(NULL);
      }
      if (after) {
	 LLMacro_GetContainer(ll, after_cont, after);
	 before_cont = after_cont->prev;
      }
      else {
	 before_cont = ll->tail.cont;
	 after_cont = NULL;
      }
      cont->data = data;
      LLMacro_ContPlaceBetween(ll, cont, before_cont, after_cont);
      ll->last = cont;
   }
   ll->count++;

   return(data);
}

/*-----------------------------------------------------------
 *  Name: 	LL_RemoveFn()
 *  Created:	Fri Sep  2 03:43:24 1994
 *  Author: 	Jonathan DeKock   <dekock@winter>
 *  DESCR:  	removes an item from the list,
 *              destroying it with function fn
 */
void LL_RemoveFn(LINKED_LIST *ll, DATA_PTR data, LL_DestroyProc destroy)
{
   if (ll->attr & LL_Intrusive) {
      if (NextP(ll, data)) PrevP(ll, NextP(ll, data)) = PrevP(ll, data);
      else ll->tail.data = PrevP(ll, data);
      if (PrevP(ll, data)) NextP(ll, PrevP(ll, data)) = NextP(ll, data);
      else ll->head.data = NextP(ll, data);
   }
   else { /* Container */
      LL_CONTAINER *cont;
      LLMacro_GetContainer(ll, cont, data);
      if (!cont) {
	 return;
      }
      if (ll->last == cont) { /* last can no longer point to this item  */
	 if (cont->prev) ll->last = cont->prev;
	 else ll->last = cont->next;
      }
      if (cont->next) cont->next->prev = cont->prev;
      else ll->tail.cont = cont->prev;
      if (cont->prev) cont->prev->next = cont->next;
      else ll->head.cont = cont->next;
      irrd_free(cont);
   }
   ll->count--;

   if (destroy) { (destroy)(data); }
}

/*-----------------------------------------------------------
 *  Name: 	LList_GetHead()
 *  Created:	Fri Sep  2 19:45:56 1994
 *  Author: 	Jonathan DeKock   <dekock@winter>
 *  DESCR:  	returns the head of a linked list
 */
DATA_PTR LList_GetHead(LINKED_LIST *ll)
{
   DATA_PTR ret;

   if (ll->attr & LL_Intrusive) {
      ret = ll->head.data;
   }
   else {
      ll->last = ll->head.cont;
      if (ll->head.cont) ret = ll->head.cont->data;
      else               ret = NULL;
   }
   return(ret);
}

/*-----------------------------------------------------------
 *  Name: 	LListGetTail()
 *  Created:	Fri Sep  2 20:11:46 1994
 *  Author: 	Jonathan DeKock   <dekock@winter>
 *  DESCR:  	gets the tail of a linked list
 */
DATA_PTR LList_GetTail(LINKED_LIST *ll)
{
   DATA_PTR ret;
   
   if (ll->attr & LL_Intrusive) {
      ret = ll->tail.data;
   }
   else {
      ll->last = ll->tail.cont;
      if (ll->tail.cont) ret = ll->tail.cont->data;
      else               ret = NULL;
   }
   
   return(ret);
}

/*-----------------------------------------------------------
 *  Name: 	LList_GetNext()
 *  Created:	Fri Sep  2 20:22:52 1994
 *  Author: 	Jonathan DeKock   <dekock@winter>
 *  DESCR:  	gets the next item on a list
 */
DATA_PTR LList_GetNext(LINKED_LIST *ll, DATA_PTR data)
{
   DATA_PTR ret;

   if (ll->attr & LL_Intrusive) {
      if (data) {
	 ret = NextP(ll, data);
      }
      else {
	 ret = ll->head.data;
      }
   }
   else {
      LL_CONTAINER *cont = NULL;
      if (data) {
	 LLMacro_GetContainer(ll, cont, data);
	 ll->last = cont->next;
	 if (cont->next) ret = cont->next->data;
	 else            ret = NULL;
      }
      else {
	 ll->last = ll->head.cont;
	 if (ll->head.cont) ret = ll->head.cont->data;
	 else               ret = NULL;
      }
   }

   return(ret);
}

/*-----------------------------------------------------------
 *  Name: 	LList_GetPrev()
 *  Created:	Fri Sep  2 23:35:25 1994
 *  Author: 	Jonathan DeKock   <dekock@winter>
 *  DESCR:  	returns the previous item
 */
DATA_PTR LList_GetPrev(LINKED_LIST *ll, DATA_PTR data)
{
   DATA_PTR ret;

   if (ll->attr & LL_Intrusive) {
      if (data) {
	 ret = PrevP(ll, data);
      }
      else {
	 ret = ll->tail.data;
      }
   }
   else {
      LL_CONTAINER *cont = NULL;
      if (data) {
	 LLMacro_GetContainer(ll, cont, data);
	 ll->last = cont->prev;
	 if (cont->prev)
	    ret = cont->prev->data;
	 else
	    ret = NULL;
      }
      else {
	 ll->last = ll->tail.cont;
	 if (ll->tail.cont) ret = ll->tail.cont->data;
	 else ret = NULL;
      }
   }
   return(ret);
}

/*-----------------------------------------------------------
 *  Name: 	LL_Sort()
 *  Created:	Sun Sep  4 01:22:27 1994
 *  Author: 	Jonathan DeKock   <dekock@winter>
 *  DESCR:  	sorts a linked list
 */
void LL_Sort(LINKED_LIST *ll, ...)
{
   LL_CompareProc compare;
   
   compare = ll->comp_fn;

   if (!compare) {
      va_list ap;
      va_start(ap, ll);
      compare = va_arg(ap, LL_CompareProc);
      va_end(ap);
   }

   LL_SortFn(ll, compare);
}

/*-----------------------------------------------------------
 *  Name: 	LL_SortFn()
 *  Created:	Sun Sep  4 01:38:01 1994
 *  Author: 	Jonathan DeKock   <dekock@winter>
 *  DESCR:  	sorts by comparison funciton given
 */
void LL_SortFn(LINKED_LIST *ll, LL_CompareProc compare)
{
   LL_SortProc sort = LL_SortFunction;
   
   sort = (LL_SortProc)LL_BubbleSort;

   /* Perform the actual sort */
   (sort)(ll, compare);
}

/*-----------------------------------------------------------
 *  Name: 	LL_BubbleSort()
 *  Created:	Sun Sep  4 01:48:33 1994
 *  Author: 	Jonathan DeKock   <dekock@winter>
 *  DESCR:  	bubble sorts a linked list
 */
void LL_BubbleSort(LINKED_LIST *ll, LL_CompareProc compare)
{
   if (ll->attr & LL_Intrusive) {
      abort();
   }
   else { /* container */
      LL_CONTAINER *bubble, *cont;
      DATA_PTR data;
      for (cont = ll->head.cont; cont; cont = cont->next) {
         for (bubble = ll->tail.cont; bubble != cont;
                                                bubble = bubble->prev) {
            if (((compare)(bubble->prev->data, bubble->data)) > 0)  {
                data = bubble->data;
                bubble->data = bubble->prev->data;
                bubble->prev->data = data;
            }
         }
      }
   }
}

/*-----------------------------------------------------------
 *  Name: 	LL_GetContainer()
 *  Created:	Thu Sep 15 19:17:22 1994
 *  Author: 	Jonathan DeKock   <dekock@winter>
 *  DESCR:  	retrieves the container for data
 */
LL_CONTAINER *LL_GetContainer(LINKED_LIST *ll, DATA_PTR data)
{
   LL_CONTAINER *cont;
   
   LLMacro_GetContainer(ll, cont, data);
   return(cont);
}
