/*-----------------------------------------------------------
 *  Name: 	hash.h
 *  Created:	Thu Sep  8 01:24:19 1994
 *  Author: 	Jonathan DeKock   <dekock@winter>
 *  DESCR:  	hash table header file
 */

#ifndef _HASH_H
#define _HASH_H

#ifdef __cplusplus
extern "C" {
#endif

#include <New.h>
#include <stdio.h>
/*#include <stdarg.h>*/
#include <linked_list.h>
   
/**************  Debug Turned ON or OFF? *****************/
#ifdef STRUCT_DEBUG
#define HASH_DEBUG
#endif

/**************  Use Prototype definitions?  *************/
#ifdef  _NO_PROTO
#define _HASH_NO_PROTO
#else
#if    !(defined(__STDC__) && __STDC__) \
    && !defined(__cplusplus) && !defined(c_plusplus) \
    && !defined(FUNCPROTO) && !defined(XTFUNCPROTO) && !defined(XMFUNCPROTO)
#define _HASH_NO_PROTO
#endif /* __STDC__ */
#endif  /*  _NO_PROTO  */

#ifdef _HASH_NO_PROTO
#define _HASH_P(ARGS) ()
#else
#define _HASH_P(ARGS) ARGS
#endif

/*********************  Attribute Definitions **********************/
#if !defined(True) && !defined(False)
#define False 0
#define True  1
#endif

enum HASH_ATTR {
   HASH_Intrusive        = (1 << 0),
   HASH_EmbeddedKey      = (1 << 1),
   HASH_ReportChange     = (1 << 2),
   HASH_ReportAccess     = (1 << 3),

   HASH_NonIntrusive     = (1 << 4),
   
   /* Offsets */
   HASH_NextOffset       = (1 << 5),
   HASH_KeyOffset,

   /* Functions */
   HASH_HashFunction,
   HASH_LookupFunction,
   HASH_DestroyFunction,

   /* optional names */
   HASH_Container        = HASH_NonIntrusive,
   HASH_NoContainer      = HASH_Intrusive
   
};

enum HASH_ERROR {
   HASH_UnknownErr,
   HASH_MemoryErr,
   HASH_TableCorrupted,
   HASH_BadIndex,
   HASH_BadArgument,
   HASH_NoMember
};

/*********************  Process Definitions  ***********************/
typedef void     (*HASH_ProcessProc)     _HASH_P((DATA_PTR));
typedef void     (*HASH_ProcessPlusProc) _HASH_P((DATA_PTR, DATA_PTR));
typedef unsigned (*HASH_HashProc)        _HASH_P((DATA_PTR, unsigned));
typedef int      (*HASH_LookupProc)      _HASH_P((DATA_PTR, DATA_PTR));
typedef void     (*HASH_DestroyProc)     _HASH_P((DATA_PTR));
typedef void     (*HASH_ErrorProc)       _HASH_P((DATA_PTR, enum HASH_ERROR, char*));
typedef void     (*HASH_PrintProc)       _HASH_P((unsigned, DATA_PTR, DATA_PTR));
    
/**********************  Structure Definitions *********************/
typedef struct _hash_container {
   struct _hash_container *next;
   DATA_PTR data;
} HASH_CONTAINER;

typedef struct _hash_table {
   union {
      DATA_PTR        *data;
      HASH_CONTAINER **cont;
   } array;

   unsigned size;
   unsigned count;
   
   unsigned short next_offset;
   unsigned short key_offset;
   
   enum HASH_ATTR attr;

   HASH_HashProc    hash;
   HASH_LookupProc  lookup;
   HASH_DestroyProc destroy;

   HASH_CONTAINER *last_cont;
} HASH_TABLE;

#define HASH_OPTIONAL_ARG ...
    
/*********************  Golbal Variables  **************************/
extern const char *HASH_errlist[];

/*********************  Function Definitions ***********************/
/*  Creation and Destruction */
extern HASH_TABLE*     HASH_Create                _HASH_P((unsigned, int, ...));
extern void            HASH_SetAttributes         _HASH_P((HASH_TABLE*, int, ...));
extern void            HASH_ClearFn               _HASH_P((HASH_TABLE*, HASH_ProcessProc));
extern void            HASH_DestroyFn             _HASH_P((HASH_TABLE*, HASH_ProcessProc));

/*  Insertion and Removal  */
extern DATA_PTR        HASH_Insert                _HASH_P((HASH_TABLE*, DATA_PTR));
extern void            HASH_RemoveFn              _HASH_P((HASH_TABLE*, DATA_PTR, HASH_DestroyProc));
extern void            HASH_RemoveByKeyFn         _HASH_P((HASH_TABLE*, DATA_PTR, HASH_DestroyProc));
/*  Lookup  */
extern DATA_PTR        HASH_Lookup                _HASH_P((HASH_TABLE*, DATA_PTR));

/*  Processing  */
extern void            HASH_Process               _HASH_P((HASH_TABLE*, HASH_ProcessProc));
extern void            HASH_ProcessPlus           _HASH_P((HASH_TABLE*, HASH_ProcessPlusProc, DATA_PTR));

/*  iteration support */
extern DATA_PTR        HASH_GetNext               _HASH_P((HASH_TABLE*, DATA_PTR));

/*  Error Handling  */
extern HASH_ErrorProc  HASH_SetHandler            _HASH_P((HASH_ErrorProc, char*));
extern void            HASH_DefaultHandler        _HASH_P((HASH_TABLE*, enum HASH_ERROR, char*));
    
/*  Debugging  */
extern int             HASH_Verify                _HASH_P((HASH_TABLE*));
extern void            HASH_Print                 _HASH_P((HASH_TABLE*, HASH_PrintProc));
extern void            HASH_DefaultPrinter        _HASH_P((unsigned, DATA_PTR, DATA_PTR));
extern HASH_CONTAINER *HASH_GetContainer          _HASH_P((HASH_TABLE*, DATA_PTR));
    
/* Default Functions */
extern unsigned        HASH_DefaultHashFunction   _HASH_P((char*, unsigned));
extern int             HASH_DefaultLookupFunction _HASH_P((char*, char*));
    
/*********************  Macros  *******************/
#define HASH_Clear(h)          HASH_ClearFn(h, (h)->destroy)
#define HASH_Destroy(h)        HASH_DestroyFn(h, (h)->destroy)
#define HASH_Remove(h, d)      HASH_RemoveFn(h, d, (h)->destroy)
#define HASH_RemoveByKey(h, d) HASH_RemoveByKeyFn(h, d, (h)->destroy)

#define HASH_SetAttribute(h, a, v) HASH_SetAttributes(h, a, v, NULL);
    
#define HASH_Offset(a, b) (((char*)b > (char*)a) ? (char*)b - (char*)a : (char*)a - (char*)b)

#define HASH_GetSize(h)   ((h)->size)
#define HASH_GetCount(h)  ((h)->count)
    
#ifndef HASH_DEBUG
#define HASH_Iterate(h, d) for (d = HASH_GetNext(h, NULL); d; d = HASH_GetNext(h, d))
#define HASH_IterateCast(h, d, type)	for (d = (type) HASH_GetNext(h, NULL); d; d = (type) HASH_GetNext(h, d))
#else
#define HASH_Iterate(h, d) \
    if (h->attr & HASH_ReportAccess) \
       printf("HASH: 0x%.8x Iterate() started.\n", h);\
    for (d = HASH_GetNext(h, NULL); d; d = HASH_GetNext(h, d))
#endif

#ifndef HASH_DEBUG
#define Set_HASH_Handler(fn) HASH_SetHandler(fn, NULL)
#else
#define Set_HASH_Handler(fn) HASH_SetHandler(fn, #fn)
#endif

#undef HASH_OPTIONAL_ARG
#ifdef _HASH_NO_PROTO
#undef _HASH_NO_PROTO
#undef _HASH_P
#endif
       
#ifdef __cplusplus
}
#endif

#endif  /*  _HASH_H  */
