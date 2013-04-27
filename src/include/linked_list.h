/*-----------------------------------------------------------
 *  Name: 	linked_list.h
 *  Created:	Fri Apr 22 02:22:39 1994
 *  Author: 	Jonathan DeKock   <dekock@winter>
 *  DESCR:  	generic linked list elements
 */


#ifndef _LINKED_LIST_H
#define _LINKED_LIST_H

#include <stdio.h>
#include <irrdmem.h>
/*#include <stdarg.h>*/
   
#ifdef __cplusplus
extern "C" {
#endif

/**************  Debug Turned ON or OFF? *****************/
#ifdef STRUCT_DEBUG
#define LL_DEBUG
#endif

/**************  Use Prototype definitions?  *************/
#ifdef  _NO_PROTO
#define _LL_NO_PROTO
#else
#if    !(defined(__STDC__) && __STDC__) \
    && !defined(__cplusplus) && !defined(c_plusplus) \
    && !defined(FUNCPROTO) && !defined(XTFUNCPROTO) && !defined(XMFUNCPROTO)
#define _LL_NO_PROTO
#endif /* __STDC__ */
#endif  /*  _NO_PROTO  */

#ifdef _LL_NO_PROTO
#define _LL_P(ARGS) ()
#else
#define _LL_P(ARGS) ARGS
#endif

/*********************  Attribute Definitions **********************/
#if !defined(True) && !defined(False)
#define False 0
#define True  1
#endif
       
enum LL_ATTR {
   /* General Attributes */
   LL_Intrusive        = (1 << 0),
   LL_AutoSort         = (1 << 1),
   LL_ReportChange     = (1 << 2),
   LL_ReportAccess     = (1 << 3),
   
   LL_NonIntrusive     = (1 << 4),
   
   /* Offsets */
   LL_NextOffset,
   LL_PrevOffset,      
   LL_PointersOffset,  

   /* Functions */
   LL_FindFunction,    
   LL_CompareFunction, 
   LL_DestroyFunction, 

   /* Optional Names */
   LL_Container       = LL_NonIntrusive,
   LL_NoContainer     = LL_Intrusive,

   /* Null Termination on list */
   LL_LastArg         = 0
};
    
/*  Errors and Warnings  */
enum LL_ERROR {
   LL_UnknownErr,
   LL_MemoryErr,
   LL_NoMember,
   LL_ListNotEmpty,
   LL_ListCorrupted,
   LL_BadOperation,
   LL_BadOffset,
   LL_BadAttributes,
   LL_BadArgument
};

/*********************  Process Definitions  ***********************/
typedef void (*LL_ProcessProc)     _LL_P((DATA_PTR));
typedef void (*LL_DestroyProc)     _LL_P((DATA_PTR));
typedef int  (*LL_CompareProc)     _LL_P((DATA_PTR, DATA_PTR));
typedef int  (*LL_FindProc)        _LL_P((DATA_PTR, DATA_PTR));
typedef int  (*LL_ProcessPlusProc) _LL_P((DATA_PTR, DATA_PTR));

/**********************  Structure Definitions *********************/
typedef struct _linked_list_container {
   struct _linked_list_container *next, *prev;
   DATA_PTR data;
} LL_CONTAINER;

typedef struct _linked_list {
   union {
      DATA_PTR          data;
      LL_CONTAINER     *cont;
   } head, tail;
   
   enum LL_ATTR  attr;
   unsigned long count;

   unsigned short    next_offset;
   unsigned short    prev_offset;
   LL_CONTAINER     *last;

   LL_FindProc    find_fn;
   LL_CompareProc comp_fn;
   LL_DestroyProc destroy_fn;
} LINKED_LIST;

typedef struct _linked_list_pointers {
   DATA_PTR next;
   DATA_PTR prev;
} LL_POINTERS;

/************  More Function Types  ********************************/
typedef int  (*LL_SortProc)        _LL_P((LINKED_LIST*, LL_CompareProc));
typedef void (*LL_ErrorProc)       _LL_P((LINKED_LIST*, enum LL_ERROR, char*));

#define LL_OPTIONAL_ARG ...    
/*********************  Golbal Variables  **************************/
extern const char *LL_errlist[];


/*********************  Function Definitions ***********************/

/*  Creation and Destruction */
extern LINKED_LIST  *LL_Create         _LL_P((int, ...));
extern void          LL_ClearFn        _LL_P((LINKED_LIST*, LL_DestroyProc));
extern void          LL_DestroyFn      _LL_P((LINKED_LIST*, LL_DestroyProc));

/*  Attribute Setting */
extern void          LL_SetAttributes  _LL_P((LINKED_LIST*, enum LL_ATTR, ...));

/*  Insertion and Removal  */
extern DATA_PTR      LL_Append         _LL_P((LINKED_LIST*, DATA_PTR));
extern DATA_PTR      LL_Prepend        _LL_P((LINKED_LIST*, DATA_PTR));
extern DATA_PTR      LL_InsertSorted   _LL_P((LINKED_LIST*, DATA_PTR, LL_OPTIONAL_ARG));
extern DATA_PTR      LL_InsertSortedFn _LL_P((LINKED_LIST*, DATA_PTR, LL_CompareProc));
extern DATA_PTR      LL_InsertAfter    _LL_P((LINKED_LIST*, DATA_PTR, DATA_PTR));
extern DATA_PTR      LL_InsertBefore   _LL_P((LINKED_LIST*, DATA_PTR, DATA_PTR));
extern void          LL_RemoveFn       _LL_P((LINKED_LIST*, DATA_PTR, LL_DestroyProc));

/*  Data Extraction  */
extern DATA_PTR      LList_GetHead     _LL_P((LINKED_LIST*));
extern DATA_PTR      LList_GetTail     _LL_P((LINKED_LIST*));
extern DATA_PTR      LList_GetNext     _LL_P((LINKED_LIST*, DATA_PTR));
extern DATA_PTR      LList_GetPrev     _LL_P((LINKED_LIST*, DATA_PTR));

/*  Search  */
extern DATA_PTR      LL_Find           _LL_P((LINKED_LIST*, DATA_PTR, LL_OPTIONAL_ARG));
extern DATA_PTR      LL_FindFromTail   _LL_P((LINKED_LIST*, DATA_PTR, LL_OPTIONAL_ARG));
extern DATA_PTR      LL_FindNext       _LL_P((LINKED_LIST*, DATA_PTR, DATA_PTR, LL_OPTIONAL_ARG));
extern DATA_PTR      LL_FindPrev       _LL_P((LINKED_LIST*, DATA_PTR, DATA_PTR, LL_OPTIONAL_ARG));

extern DATA_PTR      LL_FindFn         _LL_P((LINKED_LIST*, DATA_PTR, LL_FindProc));
extern DATA_PTR      LL_FindFromTailFn _LL_P((LINKED_LIST*, DATA_PTR, LL_FindProc));
extern DATA_PTR      LL_FindNextFn     _LL_P((LINKED_LIST*, DATA_PTR, DATA_PTR, LL_FindProc));
extern DATA_PTR      LL_FindPrevFn     _LL_P((LINKED_LIST*, DATA_PTR, DATA_PTR, LL_FindProc));

/*  Sorting  */
extern void          LL_Sort           _LL_P((LINKED_LIST*, LL_OPTIONAL_ARG));
extern void          LL_SortFn         _LL_P((LINKED_LIST*, LL_CompareProc));
extern void          LL_QuickSort      _LL_P((LINKED_LIST*, LL_CompareProc));
extern void          LL_BubbleSort     _LL_P((LINKED_LIST*, LL_CompareProc));
extern void          LL_MergeSort      _LL_P((LINKED_LIST*, LL_CompareProc));
    
/*  Processing  */
extern void          LL_Process        _LL_P((LINKED_LIST*, LL_ProcessProc));
extern void          LL_ProcessPlus    _LL_P((LINKED_LIST*, LL_ProcessPlusProc, DATA_PTR));

/*  Conversion  */
extern DATA_PTR     *LL_ToArray        _LL_P((LINKED_LIST*, DATA_PTR*, unsigned*));
extern LINKED_LIST  *LL_FromArray      _LL_P((LINKED_LIST*, DATA_PTR*, unsigned));

/*  Error Handling  */
extern LL_ErrorProc  LL_SetHandler     _LL_P((LL_ErrorProc, char*));
extern LL_SortProc   LL_SetSorter      _LL_P((LL_SortProc,  char*));
extern void          LL_DefaultHandler _LL_P((LINKED_LIST*, enum LL_ERROR, char*));
extern DATA_PTR      LL_CallHandler    _LL_P((LINKED_LIST*, enum LL_ERROR, char*));
#define LL_CallErrorHandler LL_CallHandler

/*  Debugging  */
extern int           LL_Verify         _LL_P((LINKED_LIST*));
extern void          LL_Print          _LL_P((LINKED_LIST*, LL_ProcessProc));
extern void          LL_DefaultPrinter _LL_P((DATA_PTR));
extern LL_CONTAINER *LL_GetContainer   _LL_P((LINKED_LIST*, DATA_PTR));
    
/*********************  Macros  *******************/
#define LL_Add(ll, data)     ((ll->attr & LL_AutoSort) ? LL_InsertSorted(ll, data) : LL_Append(ll, data))
#define LL_Clear(ll)         LL_ClearFn(ll, ll->destroy_fn)
#define LL_Destroy(ll)       LL_DestroyFn(ll, ll->destroy_fn)
#define LL_Remove(ll, data)  LL_RemoveFn(ll, data, ll->destroy_fn)

#define LL_ReSort(ll, data)  { LL_RemoveFn(ll, data, NULL); LL_Add(ll, data); }
    
#define LL_SetAttribute(ll, attr, val)  LL_SetAttributes(ll, attr, val, NULL)

#define LL_GetCount(ll) (ll->count)
    
#define LL_Offset(a, b) (((char*)b > (char*)a) ? (char*)b - (char*)a : (char*)a - (char*)b)

#ifndef LL_DEBUG
#define Set_LL_Handler(fn) LL_SetHandler(fn, NULL)
#define Set_LL_Sorter(fn)  LL_SetHandler(fn, NULL)
#else
#define Set_LL_Handler(fn) LL_SetHandler(fn, #fn)
#define Set_LL_Sorter(fn)  LL_SetHandler(fn, #fn)
#endif
    
/*  Data retrieval procedures  */
#ifndef LL_DEBUG 
    
#define LL_IntrGetHead(ll)    (ll->head.data)
#define LL_IntrGetTail(ll)    (ll->tail.data)
#define LL_IntrGetNext(ll, d) (*(DATA_PTR*)((char*)d + ll->next_offset))
#define LL_IntrGetPrev(ll, d) (*(DATA_PTR*)((char*)d + ll->prev_offset))

#define LL_ContGetHead(ll)    (((ll->last = ll->head.cont) != NULL) ? ll->head.cont->data : NULL) 
#define LL_ContGetTail(ll)    (((ll->last = ll->tail.cont) != NULL) ? ll->tail.cont->data : NULL) 
#ifdef notdef
#define LL_ContGetNext(ll, d) (((ll->last) && (ll->last->data == d) && ((ll->last = ll->last->next) != NULL)) ? \
			       ll->last->data : LList_GetNext(ll, d))
#define LL_ContGetPrev(ll, d) (((ll->last) && (ll->last->data == d) && ((ll->last = ll->last->prev) != NULL)) ? \
			       ll->last->data : LList_GetPrev(ll, d))
#else
/* masaki's fix: I don't think that it's required to search again
    when the last data is the same but it's next is NULL */
#define LL_ContGetNext(ll, d) (((ll->last) && (ll->last->data == d)) ? \
			       ((ll->last = ll->last->next) ? ll->last->data : NULL) : LList_GetNext(ll, d))
#define LL_ContGetPrev(ll, d) (((ll->last) && (ll->last->data == d)) ? \
			       ((ll->last = ll->last->prev) ? ll->last->data : NULL) : LList_GetPrev(ll, d))
#endif
    
#define LL_GetHead(ll)        ((ll->attr & LL_Intrusive) ? LL_IntrGetHead(ll)    : LL_ContGetHead(ll))
#define LL_GetTail(ll)        ((ll->attr & LL_Intrusive) ? LL_IntrGetTail(ll)    : LL_ContGetTail(ll))
#define LL_GetNext(ll, d)     ((ll->attr & LL_Intrusive) ? LL_IntrGetNext(ll, d) : LL_ContGetNext(ll, d))
#define LL_GetPrev(ll, d)     ((ll->attr & LL_Intrusive) ? LL_IntrGetPrev(ll, d) : LL_ContGetPrev(ll, d))

#else /* LL_DEBUG */

#define LL_IntrGetHead(ll)    ((ll->attr & LL_Intrusive) ? LList_GetHead(ll) : \
			       LL_CallErrorHandler(ll, LL_BadOperation, "LL_IntrGetHead()"))
#define LL_IntrGetTail(ll)    ((ll->attr & LL_Intrusive) ? LList_GetTail(ll) : \
			       LL_CallErrorHandler(ll, LL_BadOperation, "LL_IntrGetTail()"))
#define LL_IntrGetNext(ll, d) ((ll->attr & LL_Intrusive) ? LList_GetNext(ll, d) : \
			       LL_CallErrorHandler(ll, LL_BadOperation, "LL_IntrGetNext()"))
#define LL_IntrGetPrev(ll, d) ((ll->attr & LL_Intrusive) ? LList_GetPrev(ll, d) : \
			       LL_CallErrorHandler(ll, LL_BadOperation, "LL_IntrGetPrev()"))


#define LL_ContGetHead(ll)    ((!(ll->attr & LL_Intrusive)) ? LList_GetHead(ll) : \
			       LL_CallErrorHandler(ll, LL_BadOperation, "LL_ContGetHead()"))
#define LL_ContGetTail(ll)    ((!(ll->attr & LL_Intrusive)) ? LList_GetTail(ll) : \
			       LL_CallErrorHandler(ll, LL_BadOperation, "LL_ContGetTail()"))
#define LL_ContGetNext(ll, d) ((!(ll->attr & LL_Intrusive)) ? LList_GetNext(ll, d) : \
			       LL_CallErrorHandler(ll, LL_BadOperation, "LL_ContGetNext()"))
#define LL_ContGetPrev(ll, d) ((!(ll->attr & LL_Intrusive)) ? LList_GetPrev(ll, d) : \
			       LL_CallErrorHandler(ll, LL_BadOperation, "LL_ContGetPrev()"))
   
#define LL_GetHead(ll)        (LList_GetHead(ll))
#define LL_GetTail(ll)        (LList_GetTail(ll))
#define LL_GetNext(ll, d)     (LList_GetNext(ll, d))
#define LL_GetPrev(ll, d)     (LList_GetPrev(ll, d))

#endif  /* LL_DEBUG */

/*  Iteration Macros  */
#ifndef LL_DEBUG
   
#define LL_Iterate(ll, d)     for (d = LL_GetHead(ll);     d; d = LL_GetNext(ll, d))
#define LL_IterateCast(ll, d, type) 	for (d = (type) LL_GetHead(ll); d; d = (type) LL_GetNext(ll, d))
#define LL_IntrIterate(ll, d) for (d = LL_IntrGetHead(ll); d; d = LL_IntrGetNext(ll, d))
#define LL_ContIterate(ll, d) for (d = LL_ContGetHead(ll); d; d = LL_ContGetNext(ll, d))

#define LL_IterateBackwards(ll, d)     for (d = LL_GetTail(ll);     d; d = LL_GetPrev(ll, d))
#define LL_IntrIterateBackwards(ll, d) for (d = LL_IntrGetTail(ll); d; d = LL_IntrGetPrev(ll, d))
#define LL_ContIterateBackwards(ll, d) for (d = LL_ContGetTail(ll); d; d = LL_ContGetPrev(ll, d))
   
#define LL_IterateFindFn(ll, key, d, fn) \
   for (d = LL_FindFn(ll, key, fn); d; d = LL_FindNextFn(ll, key, d, fn))

#define LL_IterateFind(ll, key, d) \
   for (d = LL_Find(ll, key); d; d = LL_FindNext(ll, key, d))
   
#define LL_IterateFindFromTailFn(ll, key, d, fn) \
   for (d = LL_FindFromTailFn(ll, key, fn); d; d = LL_FindPrevFn(ll, key, d, fn))

#define LL_IterateFindFromTail(ll, key, d) \
   for (d = LL_FindFromTail(ll, key); d; d = LL_FindPrev(ll, key, d))


#else  /* LL_DEBUG */
   
#define LL_Iterate(ll, d)     if (ll->attr & LL_ReportAccess) \
                                 { printf("LINKED LIST: 0x%.8x Iterate() started\n", ll); } \
			      for (d = LL_GetHead(ll); d; d = LL_GetNext(ll, d))
				 
#define LL_IntrIterate(ll, d) if (!(ll->attr & LL_Intrusive)) \
				 { LL_CallErrorHandler(ll, LL_BadOperation, "LL_IntrIterate()"); } \
			      if (ll->attr & LL_ReportAccess) \
				 { printf("LINKED LIST: 0x%.8x IntrIterate() started\n", ll); } \
			      for (LL_IntrGetHead(ll); d; d = LL_IntrGetNext(ll, d))
				 
#define LL_ContIterate(ll, d) if (ll->attr & LL_Intrusive) \
				 { LL_CallErrorHandler(ll, LL_BadOperation, "LL_ContIterate()"); } \
			      if (ll->attr & LL_ReportAccess) \
				 { printf("LINKED LIST: 0x%.8x ContIterate() started\n", ll); } \
			      for (LL_ContGetHead(ll); d; d = LL_ContGetNext(ll, d))
				 
#define LL_IterateBackwards(ll, d)     if (ll->attr & LL_ReportAccess) \
                                          { printf("LINKED LIST: 0x%.8x IterateBackwards() started\n", ll); } \
				       for (d = LL_GetTail(ll); d; d = LL_GetPrev(ll, d))
				 
#define LL_IntrIterateBackwards(ll, d) if (!(ll->attr & LL_Intrusive)) \
				          { LL_CallErrorHandler(ll, LL_BadOperation, "LL_IntrIterateBackwards()"); } \
				       if (ll->attr & LL_ReportAccess) \
					  { printf("LINKED LIST: 0x%.8x IntrIterate() started\n", ll); } \
				       for (LL_IntrGetTail(ll); d; d = LL_IntrGetPrev(ll, d))
				 
#define LL_ContIterateBackwards(ll, d) if (ll->attr & LL_Intrusive) \
				          { LL_CallErrorHandler(ll, LL_BadOperation, "LL_ContIterateBackwards()"); } \
			               if (ll->attr & LL_ReportAccess) \
				          { printf("LINKED LIST: 0x%.8x ContIterate() started\n", ll); } \
			               for (LL_ContGetTail(ll); d; d = LL_ContGetPrev(ll, d))
				 
#define LL_IterateFindFn(ll, key, d, fn) \
   if (ll->attr & LL_ReportAccess) \
      { printf("LINKED LIST: 0x%.8x IterateFindFn(0x%.8x, 0x%.8x) started\n", ll, key, fn); } \
   for (d = LL_FindFn(ll, key, fn); d; d = LL_FindNextFn(ll, key, d, fn))

#define LL_IterateFind(ll, key, d) \
   if (ll->attr & LL_ReportAccess) \
      { printf("LINKED LIST: 0x%.8x IterateFind(0x%.8x) started\n", ll, key); } \
   for (d = LL_Find(ll, key); d; d = LL_FindNext(ll, key, d))
   
#define LL_IterateFindFromTailFn(ll, key, d, fn) \
   if (ll->attr & LL_ReportAccess) \
      { printf("LINKED LIST: 0x%.8x IterateFindFromTailFn(0x%.8x, 0x%.8x) started\n", ll, key, fn); } \
   for (d = LL_FindFromTailFn(ll, key, fn); d; d = LL_FindPrevFn(ll, key, d, fn))

#define LL_IterateFindFromTail(ll, key, d) \
   if (ll->attr & LL_ReportAccess) \
      { printf("LINKED LIST: 0x%.8x IterateFindFromTail(0x%.8x) started\n", ll, key); } \
   for (d = LL_FindFromTail(ll, key); d; d = LL_FindPrev(ll, key, d))
   
#endif  /* LL_DEBUG */

#undef LL_OPTIONAL_ARG
#undef _LL_NO_PROTO
#undef _LL_P
      
#ifdef __cplusplus
}
#endif

#endif  /*  _LINKED_LIST_H  */
