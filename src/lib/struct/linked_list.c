/*-----------------------------------------------------------
 *  Name: 	linked_list.c
 *  Created:	Fri Apr 22 02:31:08 1994
 *  Author: 	Jonathan DeKock   <dekock@winter>
 *  DESCR:  	generic linked list code
 */

#include <stdarg.h>
#include <New.h>
#include <array.h>
#include <linked_list.h>
#include <sys/types.h>
#include <defs.h>


/******************* Global Variables **********************/
static LL_ErrorProc   LL_Handler        = (LL_ErrorProc) LL_DefaultHandler;
static LL_SortProc    LL_SortFunction   = NULL;

const char *LL_errlist[] = {
   "unknown error",
   "memory error",
   "no such member",
   "list not empty",
   "list corrupted",
   "invalid operation",
   "invalid offset",
   "invalid set of attributes",
   "invalid argument",
};

#if defined(LL_DEBUG) || defined(_LL_INTERNAL_DEBUG)
static const char *tfs[] = {
   "False",
   "True",
};
#endif

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
       if (LL_Handler) { (LL_Handler)(ll, LL_BadArgument, "LL_SetAttributes()"); }\
       attr = (enum LL_ATTR)0;  /* inform caller of failure */\
       break;\
   }\
}

#ifdef LL_DEBUG

/*-----------------------------------------------------------
 *  Name: 	LLMacro_VerifyList()
 *  Created:	Wed Aug 31 00:47:18 1994
 *  Author: 	Jonathan DeKock   <dekock@winter>
 *  DESCR:  	in debug mode this verifies a valid list
 */
#define LLMacro_VerifyList(ll, fn) \
{\
   /* if Intrusive must have diff offsets */\
   if ((ll->attr & LL_Intrusive) && (ll->next_offset == ll->prev_offset) && (LL_Handler)) { \
      (LL_Handler)(ll, LL_BadAttributes, fn); \
   }\
   /* if Auto-sorted, must have comparison function */\
   if ((ll->attr & LL_AutoSort) && (ll->comp_fn == NULL) && (LL_Handler)) {\
      (LL_Handler)(ll, LL_BadAttributes, fn);\
   }\
}

#endif  /* LL_DEBUG */

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


/*void LLMacro_GetContainer(LINKED_LIST *ll, DATA_PTR result, DATA_PTR input) {
   if ((ll->last) && (ll->last->data == input)) {
      result = ll->last;
   }
   else if ((ll->last) && (ll->last->next != NULL) && (ll->last->next->data == input)) {
      result = ll->last->next;
   }
   else if ((ll->last) && (ll->last->prev) && (ll->last->prev->data == input)) {
      result = ll->last->prev;\
   }
   else {
     for (result = ll->head.cont; result; result = result->next) {
	 if (result->data == input) break;
     }
   }
}

*/

/*-----------------------------------------------------------
 *  Name: 	LLMacro_ContPlaceBetween()
 *  Created:	Fri Sep  2 17:29:37 1994
 *  Author: 	Jonathan DeKock   <dekock@winter>
 *  DESCR:  	places container "elt" between "before" and "after"
 */
#ifdef notdef
/*
 * Here should be a bug so that LL_Prepend doesn't work properly.
 * This routine is a macro but the arguments before and after
 * can be ll->head.cont and ll->tail.cont respectively.
 * In case that after is ll->head.cont, which will be refered
 * later in the macro, the argument after gets modofied before 
 * the refference occurs if before is null. 
 * This is the case to prepend an item. If this routine were a 
 * function, this hasn't happened.
 */
#define LLMacro_ContPlaceBetween(ll, elt, before, after)\
{\
   elt->next = after;\
   elt->prev = before;\
   if (before) before->next = elt;\
   else ll->head.cont = elt;\
   if (after) after->prev = elt;\
   else ll->tail.cont = elt;\
}
#else
/*
 * Masaki's fix
 */
#define LLMacro_ContPlaceBetween(ll, elt, before, after)\
{\
   /* This is another fix not to allow a duplicate data */\
   /*LL_CONTAINER *Xcp; */ \
   /*for (Xcp = ll->head.cont; Xcp; Xcp = Xcp->next) {\
      if (Xcp->data == elt->data) {\
	 if (LL_Handler) {\
            (LL_Handler)(ll, LL_UnknownErr, "LLMacro_ContPlaceBetween()");\
         }\
      }\ 
   }\ */ \
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
#endif


/*-----------------------------------------------------------
 *  Name: 	LLMacro_IntrPlaceBetween()
 *  Created:	Fri Sep  2 17:40:56 1994
 *  Author: 	Jonathan DeKock   <dekock@winter>
 *  DESCR:  	places data "elt" between "before" and "after"
 */
#ifdef notdef
/* See LLMacro_ContPlaceBetween -- Masaki */
#define LLMacro_IntrPlaceBetween(ll, elt, before, after)\
{\
   NextP(ll, elt) = after;\
   PrevP(ll, elt) = before;\
   if (before) NextP(ll, before) = elt;\
   else ll->head.data = elt;\
   if (after) PrevP(ll, after) = elt;\
}
#else
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
#endif

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
   if (!(ll = New(LINKED_LIST))) {
      if (LL_Handler) { (LL_Handler)(ll, LL_MemoryErr, "LL_Create()"); }
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

#ifdef LL_DEBUG
   if (ll->attr & LL_ReportChange) {
      printf("LINKED LIST: 0x%.8x Create()\n", ll);
      LLMacro_PrintAllAttr(ll);
   }
   LLMacro_VerifyList(ll, "LL_Create()");
#endif

#ifdef _LL_INTERNAL_DEBUG
   LL_Verify(ll);
#endif
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

#ifdef LL_DEBUG
   if (!ll) {
      if (LL_Handler) { (LL_Handler)(ll, LL_BadArgument, "LL_Clear()"); }
      return;
   }
#endif

   while ((data = LL_GetHead(ll))) { 
      LL_RemoveFn(ll, data, destroy);
   }
   
#ifdef LL_DEBUG
   if (ll->attr & LL_ReportChange) 
      printf("LINKED LIST: 0x%.8x Clear(0x%.8x)\n", ll, destroy);
#endif

#ifdef _LL_INTERNAL_DEBUG
   LL_Verify(ll);
#endif
}


/*-----------------------------------------------------------
 *  Name: 	LL_DestroyFn()
 *  Created:	Wed Aug 31 01:57:27 1994
 *  Author: 	Jonathan DeKock   <dekock@winter>
 *  DESCR:  	destroys a linked list and frees all Memory
 */
void LL_DestroyFn(LINKED_LIST *ll, LL_DestroyProc destroy)
{
#ifdef LL_DEBUG
   if (!ll) {
      if (LL_Handler) { (LL_Handler)(ll, LL_BadArgument, "LL_Destroy()"); }
      return;
   }
#endif

   LL_ClearFn(ll, destroy);
   Delete(ll);

#ifdef LL_DEBUG
   /*   if (ll->attr & LL_ReportChange) 
      printf("LINKED LIST: 0x%.8x Destroy(0x%.8x)\n", ll, destroy);*/

#endif
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
   
#ifdef LL_DEBUG
   if (!ll) {
      if (LL_Handler) { (LL_Handler)(ll, LL_BadArgument, "LL_SetAttributes()"); }
      return;
   }
#endif
   
   va_start(ap, first);
   for (attr = first; attr; attr = va_arg(ap, enum LL_ATTR)) {
      LLMacro_SetAttr(ll, attr, ap, val, ptr);
      if (!attr) break; /* Attribute set failure --> BAD ATTRIBUTE  */
#ifdef LL_DEBUG
      if (ll->count != 0 && ((attr & LL_Intrusive) || (attr & LL_NonIntrusive) || (attr & LL_NextOffset) ||
	  (attr & LL_PrevOffset) || (attr & LL_PointersOffset))) {
	 if (LL_Handler) { (LL_Handler)(ll, LL_ListNotEmpty, "LL_SetAttributes()");  }
	 break;
      }
#endif
   }
   va_end(ap);
   
#ifdef LL_DEBUG
   if (ll->attr & LL_ReportChange) {
      printf("LINKED LIST: 0x%.8x SetAttributes()\n", ll);
      LLMacro_PrintAllAttr(ll);
   }
   LLMacro_VerifyList(ll, "LL_SetAttributes()");
#endif
   
#ifdef _LL_INTERNAL_DEBUG
   LL_Verify(ll);
#endif
}

/*-----------------------------------------------------------
 *  Name: 	LL_Append()
 *  Created:	Wed Aug 31 16:49:07 1994
 *  Author: 	Jonathan DeKock   <dekock@winter>
 *  DESCR:  	appends a item onto the linked list
 */
DATA_PTR LL_Append(LINKED_LIST *ll, DATA_PTR data)
{
#ifdef LL_DEBUG
   if (!ll || !data) {
      if (LL_Handler) { (LL_Handler)(ll, LL_BadArgument, "LL_Append()"); }
      return(NULL);
   }
#endif
   
   if (ll->attr & LL_Intrusive) {
      LLMacro_IntrPlaceBetween(ll, data, ll->tail.data, NULL);
   } 
   else { 
      LL_CONTAINER *NULL_CONT = NULL; /* Required to make macro work */
      LL_CONTAINER *cont = New(LL_CONTAINER);
      if (!cont) {
	 if (LL_Handler) { (LL_Handler)(ll, LL_MemoryErr, "LL_Append()"); }
	 return(NULL);
      }
      cont->data = data;
      LLMacro_ContPlaceBetween(ll, cont, ll->tail.cont, NULL_CONT);
      ll->last = cont;
   }
   ll->count++;
   
#ifdef LL_DEBUG
   if (ll->attr & LL_ReportChange) {
      printf("LINKED LIST: 0x%.8x Append(0x%.8x) count = %d\n", ll, data, ll->count);
   }
#endif

#ifdef _LL_INTERNAL_DEBUG
   LL_Verify(ll);
#endif

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
#ifdef LL_DEBUG
   if (!ll || !data) {
      if (LL_Handler) { (LL_Handler)(ll, LL_BadArgument, "LL_Prepend()"); }
      return(NULL);
   }
#endif
   
   if (ll->attr & LL_Intrusive) {
      LLMacro_IntrPlaceBetween(ll, data, NULL, ll->head.data);
   }  
   else { 
      LL_CONTAINER *NULL_CONT = NULL; /* Required to make macro work */
      LL_CONTAINER *cont = New(LL_CONTAINER);
      if (!cont) {
	 if (LL_Handler) {(LL_Handler)(ll, LL_MemoryErr, "LL_Prepend()"); }
	 return(NULL);
      }
      cont->data = data;
      LLMacro_ContPlaceBetween(ll, cont, NULL_CONT, ll->head.cont);
      ll->last = cont;
   }
   ll->count++;
   
#ifdef LL_DEBUG
   if (ll->attr & LL_ReportChange) {
      printf("LINKED LIST: 0x%.8x Prepend(0x%.8x) count = %d\n", ll, data, ll->count);
   }
#endif
#ifdef _LL_INTERNAL_DEBUG
   LL_Verify(ll);
#endif
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
   
#ifdef LL_DEBUG
   if (!ll || !data) {
      if (LL_Handler) { (LL_Handler)(ll, LL_BadArgument, "LL_InsertSorted()"); }
      return(NULL);
   }
#endif
   
   if (!(compare)) {
      va_list ap;
      va_start(ap, data);
      compare = va_arg(ap, LL_CompareProc);
      va_end(ap);
   }
#ifdef LL_DEBUG
      if (!compare) {
	 if (LL_Handler) { (LL_Handler)(ll, LL_BadArgument, "LL_InsertSorted()"); }
	 return(NULL);
      }
#endif

   LL_InsertSortedFn(ll, data, compare);

#ifdef _LL_INTERNAL_DEBUG
   LL_Verify(ll);
#endif
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
   
#ifdef LL_DEBUG
   if (!ll || !data || !compare) {
      if (LL_Handler) { (LL_Handler)(ll, LL_BadArgument, "LL_InsertSortedFn()"); }
      return(NULL);
   }
#endif

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
      cont = New(LL_CONTAINER);
      if (!cont) {
	 if (LL_Handler) { (LL_Handler)(ll, LL_MemoryErr, "LL_InsertSorted()"); }
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
   
#ifdef LL_DEBUG
      if (ll->attr & LL_ReportChange) {
	 printf("LINKED LIST: 0x%.8x InsertSorted(0x%.8x, 0x%.8x) count = %d\n",
		ll, data, compare, ll->count);
      }
#endif

#ifdef _LL_INTERNAL_DEBUG
   LL_Verify(ll);
#endif
   
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
#ifdef LL_DEBUG
   if (!ll || !data) {
      if (LL_Handler) { (LL_Handler)(ll, LL_BadArgument, "LL_InsertAfter()"); }
      return(NULL);
   }
#endif
   if (ll->attr & LL_Intrusive) {
      DATA_PTR after;
      if (before) {
#ifdef LL_DEBUG
	 /* Verify that this item is on the list */
	 for (after = ll->head.data; after; after = NextP(ll, after)) {
	    if (after == before) break;
	 }
	 if (!after) {
	    if (LL_Handler) {
	       (LL_Handler)(ll, LL_NoMember, "LL_InsertAfter()");
	    }
	    return(NULL);
	 }
#endif
	 after = NextP(ll, before);
      }
      else { /* if !before */
	 after = ll->head.data;
      }
      LLMacro_IntrPlaceBetween(ll, data, before, after);
   }
   else { /* Container */
      LL_CONTAINER *after_cont, *before_cont, *cont;
      cont = New(LL_CONTAINER);
      if (!cont) {
	 if (LL_Handler) { (LL_Handler)(ll, LL_MemoryErr, "LL_InsertAfter()"); }
	 return(NULL);
      }
      if (before) {
	 LLMacro_GetContainer(ll, before_cont, before);
#ifdef LL_DEBUG
	 if (!before_cont) {
	    if (LL_Handler) { (LL_Handler)(ll, LL_NoMember, "LL_InsertAfter()"); }
	    Delete(cont);
	    return(NULL);
	 }
#endif
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

#ifdef LL_DEBUG
   if (ll->attr & LL_ReportChange) {
      printf("LINKED LIST: 0x%.8x InsertAfter(0x%.8x, 0x%.8x) count = %d\n",
	     ll, data, before, ll->count);
   }
#endif
   
#ifdef _LL_INTERNAL_DEBUG
   LL_Verify(ll);
#endif
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
#ifdef LL_DEBUG
   if (!ll || !data) {
      if (LL_Handler) { (LL_Handler)(ll, LL_BadArgument, "LL_InsertBefore()"); }
      return(NULL);
   }
#endif
   if (ll->attr & LL_Intrusive) {
      DATA_PTR before;
      if (after) {
#ifdef LL_DEBUG
	 /* Verify that this item is on the list */
	 for (before = ll->head.data; before; before = NextP(ll, before)) {
	    if (before == after) break;
	    if (before == ll->tail.data) {
	       if (LL_Handler) { (LL_Handler)(ll, LL_NoMember, "LL_InsertBefore()"); }
	       return(NULL);
	    }
	 }
#endif
	 before = PrevP(ll, after); 
      }
      else {
	 before = ll->tail.data;
      }
      LLMacro_IntrPlaceBetween(ll, data, before, after);
   }
   else { 
      LL_CONTAINER *before_cont, *after_cont, *cont;
      cont = New(LL_CONTAINER);
      if (!cont) {
	 if (LL_Handler) { (LL_Handler)(ll, LL_MemoryErr, "LL_InsertAfter()"); }
	 return(NULL);
      }
      if (after) {
	 LLMacro_GetContainer(ll, after_cont, after);
#ifdef LL_DEBUG
	 if (!after_cont) {
	    if (LL_Handler) { (LL_Handler)(ll, LL_NoMember, "LL_InsertAfter()"); }
	    Delete(cont);
	    return(NULL);
	 }
#endif
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

#ifdef LL_DEBUG
   if (ll->attr & LL_ReportChange) {
      printf("LINKED LIST: 0x%.8x InsertBefore(0x%.8x, 0x%.8x) count = %d\n",
	     ll, data, after, ll->count);
   }
#endif

#ifdef _LL_INTERNAL_DEBUG
   LL_Verify(ll);
#endif
   
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
#ifdef LL_DEBUG
   if (!ll || !data) {
      if (LL_Handler) { (LL_Handler)(ll, LL_BadArgument, "LL_Remove()"); }
      return;
   }
#endif
   if (ll->attr & LL_Intrusive) {
#ifdef LL_DEBUG
      DATA_PTR tmp;
      /* Verify that this item is on the list */
      for (tmp = ll->head.data; tmp; tmp = NextP(ll, tmp)) {
	 if (tmp == data) break;
	 if (tmp == ll->tail.data) {
	    if (LL_Handler) { (LL_Handler)(ll, LL_NoMember, "LL_Remove()"); }
	    return;
	 }
      }
#endif
      if (NextP(ll, data)) PrevP(ll, NextP(ll, data)) = PrevP(ll, data);
      else ll->tail.data = PrevP(ll, data);
      if (PrevP(ll, data)) NextP(ll, PrevP(ll, data)) = NextP(ll, data);
      else ll->head.data = NextP(ll, data);
#ifdef LL_DEBUG
      NextP(ll, data) = NULL;
      PrevP(ll, data) = NULL;
#endif
   }
   else { /* Container */
      LL_CONTAINER *cont;
      LLMacro_GetContainer(ll, cont, data);
      if (!cont) {
	 if (LL_Handler) { (LL_Handler)(ll, LL_NoMember, "LL_Remove()"); }
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
#ifdef LL_DEBUG
      cont->next = NULL;
      cont->prev = NULL;
      cont->data = NULL;
#endif
      Delete(cont);
   }
   ll->count--;

   if (destroy) { (destroy)(data); }
   
#ifdef LL_DEBUG
   if (ll->attr & LL_ReportChange) {
      printf("LINKED LIST: 0x%.8x Remove(x%.8x, 0x%.8x) count = %d\n",
	     ll, data, destroy, ll->count);
   }
#endif

#ifdef _LL_INTERNAL_DEBUG
   LL_Verify(ll);
#endif
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

#ifdef LL_DEBUG
   if (!ll) {
      if (LL_Handler) { (LL_Handler)(ll, LL_BadArgument, "LL_GetHead()"); }
      return(NULL);
   }
#endif
   
   if (ll->attr & LL_Intrusive) {
      ret = ll->head.data;
   }
   else {
      ll->last = ll->head.cont;
      if (ll->head.cont) ret = ll->head.cont->data;
      else               ret = NULL;
   }
   
#ifdef LL_DEBUG
   if (ll->attr & LL_ReportAccess) {
      printf("LINKED LIST: 0x%.8x GetHead() = 0x%.8x\n", ll, ret);
   }
#endif
   
#ifdef _LL_INTERNAL_DEBUG
   LL_Verify(ll);
#endif
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
#ifdef LL_DEBUG
   if (!ll) {
      if (LL_Handler) { (LL_Handler)(ll, LL_BadArgument, "LL_GetTail()"); }
      return(NULL);
   }
#endif
   
   if (ll->attr & LL_Intrusive) {
      ret = ll->tail.data;
   }
   else {
      ll->last = ll->tail.cont;
      if (ll->tail.cont) ret = ll->tail.cont->data;
      else               ret = NULL;
   }
   
#ifdef LL_DEBUG
   if (ll->attr & LL_ReportAccess) {
      printf("LINKED LIST: 0x%.8x GetTail() = 0x%.8x\n", ll, ret);
   }
#endif
   
#ifdef _LL_INTERNAL_DEBUG
   LL_Verify(ll);
#endif
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
#ifdef LL_DEBUG
   if (!ll) {
      if (LL_Handler) { (LL_Handler)(ll, LL_BadArgument, "LL_GetNext()"); }
      return(NULL);
   }
#endif

   if (ll->attr & LL_Intrusive) {
      if (data) {
#ifdef LL_DEBUG
	 DATA_PTR tmp;
	 /* Verify that this item is on the list */
	 for (tmp = ll->head.data; tmp; tmp = NextP(ll, tmp)) {
	    if (tmp == data) break;
	    if (tmp == ll->tail.data) {
	       if (LL_Handler) { (LL_Handler)(ll, LL_NoMember, "LL_GetNext()"); }
	       return(NULL);
	    }
	 }
#endif
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
#ifdef LL_DEBUG
	 if (!cont) { /* DATA is not on the LIST */
	    if (LL_Handler) { (LL_Handler)(ll, LL_NoMember, "LL_GetNext()"); }
	    return(NULL);
	 }
#endif
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

#ifdef LL_DEBUG
   if (ll->attr & LL_ReportAccess) {
      printf("LINKED LIST: 0x%.8x GetNext(0x%.8x) = 0x%.8x\n", ll, data, ret);
   }
#endif

#ifdef _LL_INTERNAL_DEBUG
   LL_Verify(ll);
#endif
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
#ifdef LL_DEBUG
   if (!ll) {
      if (LL_Handler) { (LL_Handler)(ll, LL_BadArgument, "LL_GetPrev()"); }
      return(NULL);
   }
#endif

   if (ll->attr & LL_Intrusive) {
      if (data) {
#ifdef LL_DEBUG
	 DATA_PTR tmp;
	 /* Verify that this item is on the list */
	 for (tmp = ll->head.data; tmp; tmp = NextP(ll, tmp)) {
	    if (tmp == data) break;
	    if (tmp == ll->tail.data) {
	       if (LL_Handler) { (LL_Handler)(ll, LL_NoMember, "LL_GetPrev()"); }
	       return(NULL);
	    }
	 }
#endif
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
#ifdef LL_DEBUG
	 if (!cont) { /* DATA is not on the LIST */
	    if (LL_Handler) { (LL_Handler)(ll, LL_NoMember, "LL_GetPrev()"); }
	    return(NULL);
	 }
#endif
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
#ifdef LL_DEBUG
   if (ll->attr & LL_ReportAccess) {
      printf("LINKED LIST: 0x%.8x GetPrev(x%.8x) = 0x%.8x\n", ll, data, ret);
   }
#endif

#ifdef _LL_INTERNAL_DEBUG
   LL_Verify(ll);
#endif
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
   
#ifdef LL_DEBUG
   if (!ll) {
      if (LL_Handler) { (LL_Handler)(ll, LL_BadArgument, "LL_Sort()");  }
      return;
   }
#endif

   compare = ll->comp_fn;

   if (!compare) {
      va_list ap;
      va_start(ap, ll);
      compare = va_arg(ap, LL_CompareProc);
      va_end(ap);
   }

#ifdef LL_DEBUG
   if (!compare) {
      if (LL_Handler) { (LL_Handler)(ll, LL_BadArgument, "LL_Sort()"); }
      return;
   }
#endif

   LL_SortFn(ll, compare);

#ifdef _LL_INTERNAL_DEBUG
   LL_Verify(ll);
#endif
   
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


#ifdef LL_DEBUG
   if (!ll || !compare) {
      if (LL_Handler) { (LL_Handler)(ll, LL_BadArgument, "LL_Sort()"); }
      return;
   }
   if ((LL_SortFunction == (LL_SortProc)LL_Sort) || (LL_SortFunction == (LL_SortProc)LL_SortFn)) {
      if (LL_Handler) { (LL_Handler)(ll, LL_BadOperation, "LL_Sort()"); }
      sort = (LL_SortProc)LL_BubbleSort;
   }
#endif
   
#ifdef notdef
   if (!sort) {
      if ((ll->attr & LL_AutoSort) || (ll->count < 30)) { sort = (LL_SortProc)LL_BubbleSort; }
      else if (ll->count < 100)                         { sort = (LL_SortProc)LL_QuickSort;  }
      else                                              { sort = (LL_SortProc)LL_MergeSort; }
   }
#else
      /* I can't beleive the other codes -- masaki */
      /* Indeed, they have some bugs. I won't fix them */
      /* I just fixed some part of bubble sort */
      sort = (LL_SortProc)LL_BubbleSort;
#endif

   /* Perform the actual sort */
   (sort)(ll, compare);

#ifdef LL_DEBUG
   if (ll->attr & LL_ReportChange) {
      printf("LINKED LIST: 0x%.8x Sort(0x%.8x) complete\n", ll, compare);
   }
#endif
   
#ifdef _LL_INTERNAL_DEBUG
   LL_Verify(ll);
#endif
}

/*-----------------------------------------------------------
 *  Name: 	LL_BubbleSort()
 *  Created:	Sun Sep  4 01:48:33 1994
 *  Author: 	Jonathan DeKock   <dekock@winter>
 *  DESCR:  	bubble sorts a linked list
 */
void LL_BubbleSort(LINKED_LIST *ll, LL_CompareProc compare)
{
#ifdef LL_DEBUG
   if (!ll || !compare) {
      if (LL_Handler) { (LL_Handler)(ll, LL_BadArgument, "LL_BubbleSort()"); }
      return;
   }
#endif

   if (ll->attr & LL_Intrusive) {
#ifdef notdef
      DATA_PTR data, bubble;
      for (data = NextP(ll, ll->head.data); data; data = NextP(ll, data)) {
	 for (bubble = PrevP(ll, data); bubble; bubble = PrevP(ll, bubble)) {
	    if (((compare)(bubble, data)) < 0) break;
	 }
	 if (bubble && bubble != PrevP(ll, data)) { /* it has moved */
	    /* unlink it */
	    if (NextP(ll, data)) PrevP(ll, NextP(ll, data)) = PrevP(ll, data);
	    else ll->tail.data = PrevP(ll, data);
	    if (PrevP(ll, data)) NextP(ll, PrevP(ll, data)) = NextP(ll, data);
	    else ll->head.data = NextP(ll, data);
	    /* re-link it */
	    if (bubble) { LLMacro_IntrPlaceBetween(ll, data, bubble, NextP(ll, bubble)); }
	    else        { LLMacro_IntrPlaceBetween(ll, data, bubble, ll->head.data);     }
	 }
      }
#else
      /* The above code is incorrect. Should not be used. -- masaki */
      abort();
#endif 
   }
   else { /* container */
      LL_CONTAINER *bubble, *cont;
      DATA_PTR data;
#ifdef notdef
      for (cont = ll->head.cont->next; cont; cont = cont->next) {
	 for (bubble = cont->prev; bubble; bubble = bubble->prev) {
	    if (((compare)(bubble->data, cont->data)) < 0)  break;
	 }
	 if (bubble && bubble != cont->prev) { /* it has moved */
	    /* switch the data pointers around */
	    data = bubble->data;
	    bubble->data = cont->data;
	    cont->data = data;
	 }
      }
#else
      /* I can't imagine what the above code intended to -- masaki */

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
#endif
   }

#ifdef LL_DEBUG
   if (ll->attr & LL_ReportChange) {
      printf("LINKED LIST: 0x%.8x BubbleSort(0x%.8x) complete\n", ll, compare);
   }
#endif

#ifdef _LL_INTERNAL_DEBUG
   LL_Verify(ll);
#endif
}

#ifdef notdef /* not currently supported */
/*-----------------------------------------------------------
 *  Name: 	LL_QuickSort()
 *  Created:	Sun Sep  4 02:18:00 1994
 *  Author: 	Jonathan DeKock   <dekock@winter>
 *  DESCR:  	quick sorts an linked list
 */
void LL_QuickSort(LINKED_LIST *ll, LL_CompareProc compare)
{
   DATA_PTR *array = NULL;
   
#ifdef LL_DEBUG
   if (!ll || !compare) {
      if (LL_Handler) { (LL_Handler)(ll, LL_BadArgument, "LL_QuickSort()"); }
      return;
   }
#endif

   if (ll->count <= 1) return; /* nothing to sort */

   /* Create a big enough array */
   if (!(LL_SortArraySize)) {
      array = NewArray(DATA_PTR, ll->count);
      if (array) {
	 LL_SortArraySize = ll->count;
	 LL_SortArray     = array;
      }
   }
   else if (LL_SortArraySize < ll->count) {
      array = ReallocateArray(LL_SortArray, DATA_PTR, ll->count);
      if (array) {
	 LL_SortArraySize = ll->count;
	 LL_SortArray     = array;
      }
      else {
	 Delete(LL_SortArray); /* no memory, get rid of this */
	 LL_SortArray = NULL;
	 LL_SortArraySize = 0;
      }
   }

   /* Make sure we didn't have a memory error */
   if (!LL_SortArray) { LL_BubbleSort(ll, compare); return; }

   /* Actual array conversion, sort, and re-conversion */
   LL_ToArray(ll, LL_SortArray, NULL);
   ARRAY_QuickSort(LL_SortArray, ll->count, (DATA_PTR)compare);              
   LL_FromArray(ll, LL_SortArray, ll->count);

#ifdef LL_DEBUG
   if (ll->attr & LL_ReportChange) {
      printf("LINKED LIST: 0x%.8x QuickSort(0x%.8x) complete\n", ll, compare);
   }
#endif

#ifdef _LL_INTERNAL_DEBUG
   LL_Verify(ll);
#endif
}


/*-----------------------------------------------------------
 *  Name: 	LL_MergeSort()
 *  Created:	Sun Sep  4 02:19:12 1994
 *  Author: 	Jonathan DeKock   <dekock@winter>
 *  DESCR:  	merge sorts an array
 */
void LL_MergeSort(LINKED_LIST *ll, LL_CompareProc compare)
{
   DATA_PTR *array = NULL;
   
#ifdef LL_DEBUG
   if (!ll || !compare) {
      if (LL_Handler) { (LL_Handler)(ll, LL_BadArgument, "LL_MergeSort()"); }
      return;
   }
#endif
   
   if (!ll->count) return; /* nothing to sort */

   /* Create a big enough array */
   if (!(LL_SortArraySize)) {
      array = NewArray(DATA_PTR, ll->count);
      if (array) {
	 LL_SortArraySize = ll->count;
	 LL_SortArray     = array;
      }
   }
   else if (LL_SortArraySize < ll->count) {
      array = ReallocateArray(LL_SortArray, DATA_PTR, ll->count);
      if (array) {
	 LL_SortArraySize = ll->count;
	 LL_SortArray     = array;
      }
      else {
	 Delete(LL_SortArray); /* no memory, get rid of this */
	 LL_SortArray = NULL;
	 LL_SortArraySize = 0;
      }
   }

   /* Make sure we didn't have a memory error */
   if (!LL_SortArray) { LL_BubbleSort(ll, compare); return; }

   /* Actual conversion, sort, and re-conversion */
   LL_ToArray(ll, LL_SortArray, NULL);
   ARRAY_MergeSort(LL_SortArray, ll->count, (DATA_PTR)compare);              
   LL_FromArray(ll, LL_SortArray, ll->count);

#ifdef LL_DEBUG
   if (ll->attr & LL_ReportChange) {
      printf("LINKED LIST: 0x%.8x MergeSort(0x%.8x) complete\n", ll, compare);
   }
#endif

#ifdef _LL_INTERNAL_DEBUG
   LL_Verify(ll);
#endif
}

#endif

#ifdef _LL_INTERNAL_DEBUG

/*-----------------------------------------------------------
 *  Name: 	LL_Process()
 *  Created:	Sun Sep  4 03:50:17 1994
 *  Author: 	Jonathan DeKock   <dekock@winter>
 *  DESCR:  	runs function <fn> on each item of the list
 */
void LL_Process(LINKED_LIST *ll, LL_ProcessProc fn)
{
#ifdef LL_DEBUG
   if (!ll || !fn) {
      if (LL_Handler) { (LL_Handler)(ll, LL_BadArgument, "LL_Process()"); }
      return;
   }
   if (ll->attr & LL_ReportAccess) {
      printf("LINKED LIST: 0x%.8x Process(0x%.8x) started\n", ll, fn);
   }
#endif
   
   if (ll->attr & LL_Intrusive) {
      DATA_PTR data;
      for (data = ll->head.data; data; data = NextP(ll, data)) {
	 (fn)(data);
      }
   }
   else {
      LL_CONTAINER *cont;
      for (cont = ll->head.cont; cont; cont = cont->next) {
	 (fn)(cont->data);
      }
   }
#ifdef LL_DEBUG
   if (ll->attr & LL_ReportAccess) {
      printf("LINKED LIST: 0x%.8x Process(0x%.8x) complete\n", ll, fn);
   }
#endif
   
#ifdef _LL_INTERNAL_DEBUG
   LL_Verify(ll);
#endif
}


/*-----------------------------------------------------------
 *  Name: 	LL_ProcessPlus()
 *  Created:	Sun Sep  4 03:55:50 1994
 *  Author: 	Jonathan DeKock   <dekock@winter>
 *  DESCR:  	funs <fn> with <arg2> across every item of list
 */
void LL_ProcessPlus(LINKED_LIST *ll, LL_ProcessPlusProc fn, DATA_PTR arg2)
{
#ifdef LL_DEBUG
   if (!ll || !fn) {
      if (LL_Handler) { (LL_Handler)(ll, LL_BadArgument, "LL_ProcessPlus()"); }
      return;
   }
   if (ll->attr & LL_ReportAccess) {
      printf("LINKED LIST: 0x%.8x ProcessPlus(0x%.8x, 0x%.8x) started\n", ll, fn, arg2);
   }
#endif

   if (ll->attr & LL_Intrusive) {
      DATA_PTR data;
      for (data = ll->head.data; data; data = NextP(ll, data)) {
	 (fn)(data, arg2);
      }
   }
   else {
      LL_CONTAINER *cont;
      for (cont = ll->head.cont; cont; cont = cont->next) {
	 (fn)(cont->data, arg2);
      }
   }
#ifdef LL_DEBUG
   if (ll->attr & LL_ReportAccess) {
      printf("LINKED LIST: 0x%.8x ProcessPlus(0x%.8x, 0x%.8x) complete\n", ll, fn, arg2);
   }
#endif
#ifdef _LL_INTERNAL_DEBUG
   LL_Verify(ll);
#endif
} 

#endif /* _LL_INTERNAL_DEBUG */

#ifdef notdef  /* used by quicksort code, currently not used -ljb */

/*-----------------------------------------------------------
 *  Name: 	LL_ToArray()
 *  Created:	Sun Sep  4 03:58:52 1994
 *  Author: 	Jonathan DeKock   <dekock@winter>
 *  DESCR:  	converts a linked list to an array
 */
DATA_PTR *LL_ToArray(LINKED_LIST *ll, DATA_PTR *array, unsigned *size)
{
   u_int i;
#ifdef LL_DEBUG
   if (!ll) {
      if (LL_Handler) { (LL_Handler)(ll, LL_BadArgument, "LL_ToArray()"); }
      return(NULL);
   }
#endif

   if (!array) {
      array = NewArray(DATA_PTR, ll->count);
      if (!array) {
	 if (LL_Handler) { (LL_Handler)(ll, LL_MemoryErr, "LL_ToArray()"); }
	 return(NULL);
      }
   }

   if (ll->attr & LL_Intrusive) {
      DATA_PTR data;
      for (i = 0, data = ll->head.data; i < ll->count; data = NextP(ll, data), i++) {
	 array[i] = data;
      }
   }
   else {
      LL_CONTAINER *cont;
      for (i = 0, cont = ll->head.cont; i < ll->count; cont = cont->next, i++) {
	 array[i] = cont->data;
      }
   }
   
   if (size) *size = ll->count;

#ifdef LL_DEBUG
   if (ll->attr & LL_ReportAccess) {
      printf("LINKED LIST: 0x%.8x ToArray(0x%.8x, %d) complete\n", ll, array, ll->count);
   }
#endif
   
#ifdef _LL_INTERNAL_DEBUG
   LL_Verify(ll);
#endif

   return(array);
}


/*-----------------------------------------------------------
 *  Name: 	LL_FromArray()
 *  Created:	Sun Sep  4 04:10:39 1994
 *  Author: 	Jonathan DeKock   <dekock@winter>
 *  DESCR:  	converts an array to a linked list
 */
LINKED_LIST *LL_FromArray(LINKED_LIST *ll, DATA_PTR *array, unsigned size)
{
   u_int i;
   
#ifdef LL_DEBUG
   if (!ll) {
      if (LL_Handler) { (LL_Handler)(ll, LL_BadArgument, "LL_FromArray()"); }
      return(NULL);
   }
#endif
   if (!ll) ll = LL_Create(0);

   if (ll->attr & LL_Intrusive) {
      if (size > 0) {
	 ll->head.data = array[0];
	 ll->tail.data = array[size-1];
      }
      if (ll->head.data) {
	 PrevP(ll, ll->head.data) = NULL;
	 NextP(ll, ll->head.data) = (size > 1) ? array[1] : NULL;

      }
      if (ll->tail.data) {
	 NextP(ll, ll->tail.data) = NULL;
	 PrevP(ll, ll->tail.data) = (size > 1) ? array[size-2] : NULL;
      }
      for (i = 1; i < size-1; i++) {
	 NextP(ll, array[i]) = array[i+1];
	 PrevP(ll, array[i]) = array[i-1];
      }
   }
   else if (size && (ll->count == size)) {
      LL_CONTAINER *cont;
      for (cont = ll->head.cont, i=0; i < size; cont = cont->next, i++)
	 cont->data = array[i];
   }
   else {
      LL_ClearFn(ll, NULL);
      ll->count = 0;
      ll->head.cont = NULL;
      ll->tail.cont = NULL;
      for (i = 0; i < size; i++) {
	 LL_Append(ll, array[i]);
      }
   }
   ll->count = size;
   
#ifdef LL_DEBUG
   if (ll->attr & LL_ReportAccess) {
      printf("LINKED LIST: 0x%.8x FromArray(0x%.8x, %d) complete\n", ll, array, ll->count);
   }
#endif
   
#ifdef _LL_INTERNAL_DEBUG
   LL_Verify(ll);
#endif

   return(ll);
}

#endif

/*-----------------------------------------------------------
 *  Name: 	LL_SetHandler()
 *  Created:	Sun Sep  4 06:33:10 1994
 *  Author: 	Jonathan DeKock   <dekock@winter>
 *  DESCR:  	sets the error handler to <fn>
 */
LL_ErrorProc LL_SetHandler(LL_ErrorProc fn, char *name)
{
   LL_ErrorProc tmp= LL_Handler;
   LL_Handler = fn;
#ifdef LL_DEBUG
   if (name) printf("LINKED LIST: Handler changed to %s() = 0x%.8x\n", name, fn);
   else      printf("LINKED LIST: Handler changed to 0x%.8x\n", fn);
#endif
   return(tmp);
}


#ifdef notdef /* not currently supported */
/*-----------------------------------------------------------
 *  Name: 	LL_SetSorter()
 *  Created:	Wed Sep 14 04:24:59 1994
 *  Author: 	Jonathan DeKock   <dekock@winter>
 *  DESCR:  	sets the global sorter function
 */
LL_SortProc LL_SetSorter(LL_SortProc fn, char *name)
{
   LL_SortProc tmp = LL_SortFunction;
   LL_SortFunction = fn;
#ifdef LL_DEBUG
   if (name) printf("LINKED LIST: Sorter changed to %s() = 0x%.8x\n", name, fn);
   else      printf("LINKED LIST: Sorter changed to 0x%.8x\n", fn);
#endif
   return(tmp);
}
#endif

/*-----------------------------------------------------------	  
 *  Name: 	LL_DefaultHandler()
 *  Created:	Tue Aug 30 19:37:58 1994
 *  Author: 	Jonathan DeKock   <dekock@winter>
 *  DESCR:  	default error handler
 */
void LL_DefaultHandler(LINKED_LIST *ll, enum LL_ERROR num, char *name)
{
#ifdef LL_DEBUG
   FILE *out = stdout;
#else
   FILE *out = stderr;
#endif
   fflush(stdout);
   fflush(stderr);
   fprintf(out, "LINKED LIST: 0x%.8x Error in function %s: \"%s\"\n", (u_int)ll, name, LL_errlist[num]);
   fflush(out);
}

/*-----------------------------------------------------------
 *  Name: 	LL_CallHandler()
 *  Created:	Sat Sep  3 00:18:54 1994
 *  Author: 	Jonathan DeKock   <dekock@winter>
 *  DESCR:  	calls the error handler
 *              used as interface to get to the
 *              _static_ "global" variable
 */
DATA_PTR LL_CallHandler(LINKED_LIST *ll, enum LL_ERROR num, char *name)
{
   if (LL_Handler) { (LL_Handler)(ll, num, name); }
   return(NULL);
}

#ifdef _LL_INTERNAL_DEBUG

/*-----------------------------------------------------------
 *  Name: 	LL_Verify()
 *  Created:	Sun Sep  4 06:53:54 1994
 *  Author: 	Jonathan DeKock   <dekock@winter>
 *  DESCR:  	verifies a list
 */
int LL_Verify(LINKED_LIST *ll)
{
   int           ret = 1;
   u_int         count = 0;
   int           last_found = 0;
   DATA_PTR      data;
   LL_CONTAINER *cont;
   
   /* Are the attributes correct????  */
   /* if Intrusive, must have diff offsets */
   if ((ll->attr & LL_Intrusive) && (ll->next_offset == ll->prev_offset)) {
      if (LL_Handler) { (LL_Handler)(ll, LL_BadAttributes, "LL_Verify()"); }
      ret = 0;
#ifdef _LL_INTERNAL_DEBUG
      printf("LINKED LIST: 0x%.8x Verify() found (prev_offset == next_offset) = %d\n",
	     ll, ll->prev_offset);
#endif
   }
   
   /* if Auto-sorted, must have comparison function */
   if ((ll->attr & LL_AutoSort) && (ll->comp_fn == NULL)) {
      if (LL_Handler) { (LL_Handler)(ll, LL_BadAttributes, "LL_Verify()"); }
      ret = 0;
#ifdef _LL_INTERNAL_DEBUG
      printf("LINKED LIST: 0x%.8x Verify() found auto-sort, but no comp_fn\n", ll);
#endif
   }
   
   /* If the list is empty, are the pointers NULL ? */
   if (!ll->count) {
      if ((ll->head.data) || (ll->tail.data) || (ll->last)) {
	 if (LL_Handler) { (LL_Handler)(ll, LL_ListCorrupted, "LL_Verify()"); }
	 ret = 0;
#ifdef _LL_INTERNAL_DEBUG
	 printf("LINKED LIST: 0x%.8x Verify() found non-empty head or tail on empty list\n", ll);
#endif
      }
   }
   else { /* list not empty */
      if (!(ll->head.data) || !(ll->tail.data)) {
	 if (LL_Handler) { (LL_Handler)(ll, LL_ListCorrupted, "LL_Verify()"); }
	 ret = 0;
#ifdef _LL_INTERNAL_DEBUG
	 printf("LINKED LIST: 0x%.8x Verify() found empty head or tail on non-empty list\n", ll);
#endif
      }
      
      if (ll->attr & LL_Intrusive) {
	 if (NextP(ll, ll->tail.data) != NULL) {
	    if (LL_Handler) { (LL_Handler)(ll, LL_ListCorrupted, "LL_Verify()"); }
	    ret = 0;
#ifdef _LL_INTERNAL_DEBUG
	    printf("LINKED LIST: 0x%.8x Verify() found invalid next pointer on tail\n", ll);
#endif
	 }
	 if (PrevP(ll, ll->head.data) != NULL) {
	    if (LL_Handler) { (LL_Handler)(ll, LL_ListCorrupted, "LL_Verify()"); }
	    ret = 0;
#ifdef _LL_INTERNAL_DEBUG
	    printf("LINKED LIST: 0x%.8x Verify() found invalid prev pointer on head\n", ll);
#endif
	 }

	 for (data = ll->head.data; data; data = NextP(ll, data)) {
	    count++;
	    /* Check that data->next->prev == data */
	    if ((!(NextP(ll, data))) && (data != ll->tail.data)) {
	       /* list quit to soon */
	       if (LL_Handler) { (LL_Handler)(ll, LL_ListCorrupted, "LL_Verify()"); }
	       ret = 0;
#ifdef _LL_INTERNAL_DEBUG
	       printf("LINKED LIST: 0x%.8x Verify() found a NULL next pointer mid-list\n", ll);
#endif
	    }
	    else if ((NextP(ll, data)) && (PrevP(ll, NextP(ll, data)) != data)) {
	       if (LL_Handler) { (LL_Handler)(ll, LL_ListCorrupted, "LL_Verify()"); }
	       ret = 0;
#ifdef _LL_INTERNAL_DEBUG
	       printf("LINKED LIST: 0x%.8x Verify() found data->next->prev != data\n", ll);
#endif
	    }
	    if (ll->attr & LL_AutoSort) {
	       if ((ll->comp_fn)(data, NextP(ll, data)) > 0) { /* Not Sorted */
		  if (LL_Handler) { (LL_Handler)(ll, LL_ListCorrupted, "LL_Verify()"); }
		  ret = 0;
#ifdef _LL_INTERNAL_DEBUG
		  printf("LINKED LIST: 0x%.8x Verify() found an auto-sorted list un-sorted\n", ll);
#endif
	       }
	    }
	 }
	 if (count != ll->count) {
	    if (LL_Handler) { (LL_Handler)(ll, LL_ListCorrupted, "LL_Verify()"); }
	    ret = 0;
#ifdef _LL_INTERNAL_DEBUG
	    printf("LINKED LIST: 0x%.8x Verify() found the count incorrect\n", ll);
#endif
	 }
      }
      else { /* container */
	 if (ll->tail.cont->next != NULL) {
	    if (LL_Handler) { (LL_Handler)(ll, LL_ListCorrupted, "LL_Verify()"); }
	    ret = 0;
#ifdef _LL_INTERNAL_DEBUG
	    printf("LINKED LIST: 0x%.8x Verify() found invalid next pointer on tail\n", ll);
#endif
	 }
	 if (ll->head.cont->prev != NULL) {
	    if (LL_Handler) { (LL_Handler)(ll, LL_ListCorrupted, "LL_Verify()"); }
	    ret = 0;
#ifdef _LL_INTERNAL_DEBUG
	    printf("LINKED LIST: 0x%.8x Verify() found invalid prev pointer on head\n", ll);
#endif
	 }
	 if (!ll->last) last_found = 1;
	 for (cont = ll->head.cont; cont; cont = cont->next) {
	    count++;
	    if (!cont->data) {
	       if (LL_Handler) { (LL_Handler)(ll, LL_ListCorrupted, "LL_Verify()"); }
	       ret = 0;
#ifdef _LL_INTERNAL_DEBUG
	       printf("LINKED LIST: 0x%.8x Verify() found a container with NULL data pointer\n", ll);
#endif
	    }
	    /* Check that cont->next->prev == cont */
	    if ((!cont->next) && (cont != ll->tail.cont)) {
	       /* list quit to soon */
	       if (LL_Handler) { (LL_Handler)(ll, LL_ListCorrupted, "LL_Verify()"); }
	       ret = 0;
#ifdef _LL_INTERNAL_DEBUG
	       printf("LINKED LIST: 0x%.8x Verify() found a NULL next pointer mid-list\n", ll);
#endif
	    }
	    else if (cont->next && (cont->next->prev != cont)) {
	       if (LL_Handler) { (LL_Handler)(ll, LL_ListCorrupted, "LL_Verify()"); }
	       ret = 0;
#ifdef _LL_INTERNAL_DEBUG
	       printf("LINKED LIST: 0x%.8x Verify() found cont->next->prev != cont\n", ll);
#endif
	    }
	    if (ll->attr & LL_AutoSort) {
	       if (cont->next && (ll->comp_fn)(cont->data, cont->next->data) > 0) { /* Not Sorted */
		  if (LL_Handler) { (LL_Handler)(ll, LL_ListCorrupted, "LL_Verify()"); }
		  ret = 0;
#ifdef _LL_INTERNAL_DEBUG
		  printf("LINKED LIST: 0x%.8x Verify() found an auto-sorted list un-sorted\n", ll);
#endif
	       }
	    }
	    if (ll->last == cont) last_found = 1;
	 }
	 if (count != ll->count) {
	    if (LL_Handler) { (LL_Handler)(ll, LL_ListCorrupted, "LL_Verify()"); }
	    ret = 0;
#ifdef _LL_INTERNAL_DEBUG
	    printf("LINKED LIST: 0x%.8x Verify() found the count incorrect\n", ll);
#endif
	 }
	 if (!last_found) {
	    if (LL_Handler) { (LL_Handler)(ll, LL_ListCorrupted, "LL_Verify()"); }
	    ret = 0;
#ifdef _LL_INTERNAL_DEBUG
	    printf("LINKED LIST: 0x%.8x Verify() found an invalid last argument\n", ll);
#endif
	 }
      }
   }
   
#ifdef _LL_INTERNAL_DEBUG
   if (!ret) printf("LINKED LIST: 0x%.8x is NOT valid\n", ll);
#endif
   
   return(ret);
}


/*-----------------------------------------------------------
 *  Name: 	LL_Print()
 *  Created:	Sun Sep  4 08:00:05 1994
 *  Author: 	Jonathan DeKock   <dekock@winter>
 *  DESCR:  	verifies and prints a list
 */
void LL_Print(LINKED_LIST *ll, LL_ProcessProc print)
{
#ifdef LL_DEBUG
   if (!ll) {
      if (LL_Handler) { (LL_Handler)(ll, LL_BadArgument, "LL_PrintListDataToFile()"); }
      return;
   }
#endif
   LL_Verify(ll);
   if (!print) print = (LL_ProcessProc) LL_DefaultPrinter;
   printf("LINKED LIST: 0x%.8x Printing..\n", (u_int)ll);
   LLMacro_PrintAllAttr(ll);
   LL_Process(ll, print);
}



/*-----------------------------------------------------------
 *  Name: 	LL_DefaultPrinter
 *  Created:	Wed Sep 14 04:31:58 1994
 *  Author: 	Jonathan DeKock   <dekock@winter>
 *  DESCR:  	default printing funciton
 */
void LL_DefaultPrinter(DATA_PTR data)
{
   printf("0x%.8x\n", (u_int)data);
}

#endif /* _LL_INTERNAL_DEBUG */

/*-----------------------------------------------------------
 *  Name: 	LL_GetContainer()
 *  Created:	Thu Sep 15 19:17:22 1994
 *  Author: 	Jonathan DeKock   <dekock@winter>
 *  DESCR:  	retrieves the container for data
 */
LL_CONTAINER *LL_GetContainer(LINKED_LIST *ll, DATA_PTR data)
{
   LL_CONTAINER *cont;
   
#ifdef LL_DEBUG
   if (!ll || !data) {
      if (LL_Handler) { (LL_Handler)(ll, LL_BadArgument, "LL_GetContainer()"); }
      return(NULL);
   }
   if (ll->attr & LL_Intrusive) {
      if (LL_Handler) { (LL_Handler)(ll, LL_BadOperation, "LL_GetContainer()"); }
      return(NULL);
   }
#endif
   
   LLMacro_GetContainer(ll, cont, data);
   return(cont);
}
