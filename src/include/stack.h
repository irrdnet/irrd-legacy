/*-----------------------------------------------------------
 *  Name: 	stack.h
 *  Created:	Fri Aug 26 17:45:02 1994
 *  Author: 	Jonathan DeKock   <dekock@winter>
 *  DESCR:  	provides a stack of elements
 */

#ifndef _STACK_H
#define _STACK_H

#include <New.h>
   
#ifdef __cplusplus
extern "C" {
#define STACK_EXPLICIT_TYPE
#endif

/**************  Debug Turned ON or OFF? *****************/
#ifdef STRUCT_DEBUG
#define STACK_DEBUG
#endif

/**************  Use Prototype definitions?  *************/
#ifdef  _NO_PROTO
#define _STACK_NO_PROTO
#else
#if    !(defined(__STDC__) && __STDC__) \
    && !defined(__cplusplus) && !defined(c_plusplus) \
    && !defined(FUNCPROTO) && !defined(XTFUNCPROTO) && !defined(XMFUNCPROTO)
#define _STACK_NO_PROTO
#endif /* __STDC__ */
#endif  /*  _NO_PROTO  */

#ifdef _STACK_NO_PROTO
#define _STACK_P(ARGS) ()
#else
#define _STACK_P(ARGS) ARGS
#endif

/*************  Global Variables **********************/
extern const char *STACK_errlist[];

/*************  Definitions  **************************/
enum STACK_ATTR {
   STACK_DYNAMIC = (1 << 0),
   STACK_REPORT  = (1 << 1)
};

enum STACK_ERROR {
   STACK_UnknownErr,
   STACK_MemoryErr,
   STACK_Full,
   STACK_Empty,
   STACK_BadArgument
};
    
#if !defined(True) && !defined(False)
#define True  1
#define False 0
#endif
    
/*************  Type Definitions  *********************/
typedef long STACK_TYPE;

typedef struct stack {
   unsigned long size;
   unsigned long top;
   STACK_TYPE *array;
   enum STACK_ATTR attr;
} STACK;

/*************  Prototypes   **************************/
typedef void (*STACK_ErrorProc)   _STACK_P((STACK*, int, char*));
typedef void (*STACK_PrintProc)   _STACK_P((int, STACK_TYPE));

/*************  Function Definitions  *****************/
extern STACK*          STACK_Create         _STACK_P((unsigned));
extern void            STACK_Destroy        _STACK_P((STACK*));

extern void            STACK_Push           _STACK_P((STACK*, STACK_TYPE));
extern STACK_TYPE      STACK_Pop            _STACK_P((STACK*));

extern STACK_ErrorProc STACK_SetHandler     _STACK_P((STACK_ErrorProc, char*));
extern void            STACK_DefaultHandler _STACK_P((STACK*, enum STACK_ERROR, char*));

extern void            STACK_Print          _STACK_P((STACK*, STACK_PrintProc));
extern void            STACK_DefaultPrintFn _STACK_P((unsigned, STACK_TYPE));

/************  Macros  *******************************/
#define Create_STACK(size)        STACK_Create(size)
#define Destroy_STACK(s)          STACK_Destroy(s)
#define Clear_STACK(s)            (s->top = 0)
#define Print_STACK(s, fn)        STACK_Print(s, (STACK_PrintProc)fn)
#define Set_STACK_Dynamic(s, val) (s->attr = (enum STACK_ATTR)((val) ? s->attr | STACK_DYNAMIC : s->attr & ~STACK_DYNAMIC))
#define Get_STACK_Dynamic(s)      (s->attr & STACK_Dynamic)
#define Set_STACK_Report(s, val)  (s->attr = (enum STACK_ATTR)((val) ? s->attr | STACK_REPORT  : s->attr & ~STACK_REPORT))
#define Get_STACK_Report(s)       (s->attr >> 1)
#define Get_STACK_Count(s)        (s->top)
#define Get_STACK_Size(s)         (s->size)

/*  Fast Macro Push and Pop for static sized stacks _only_ */
#define PushM(s, d)          (s->array[s->top++] = (STACK_TYPE)d)
#define PopM(s)              (s->array[--s->top])

#ifdef STACK_DEBUG  /*  Debugging ON  */
#define Push(s, d)            STACK_Push(s, (STACK_TYPE)d)
#define Pop(s)                STACK_Pop(s)
#define Set_STACK_Handler(fn) STACK_SetHandler((STACK_ErrorProc)fn, #fn)
#else               /*  Debugging OFF  */
#define Push(s, d)            ((s->top != s->size) ? PushM(s, d) : STACK_Push(s, (STACK_TYPE)d))
#define Pop(s)                ((s->top != 0) ? PopM(s) : STACK_Pop(s))
#define Set_STACK_Handler(fn) STACK_SetHandler((STACK_ErrorProc)fn, NULL)
#endif

    
#define MultiPush(s, d)      {  STACK_TYPE *stack_type_ptr = (STACK_TYPE*)&d; int stack_type_index;\
				for(stack_type_index = 0;\
				    stack_type_index < (sizeof(d) + sizeof(STACK_TYPE) - 1)/sizeof(STACK_TYPE);\
				    stack_type_index++) {\
				   Push(s, stack_type_ptr[stack_type_index]);\
				}\
			     }
#define MultiPop(s, d)       {  STACK_TYPE *stack_type_ptr = (STACK_TYPE*)&d; int stack_type_index;\
				for(stack_type_index =  ((sizeof(d) + sizeof(STACK_TYPE) - 1)/sizeof(STACK_TYPE)) - 1;\
				    stack_type_index >= 0;\
				    stack_type_index--) {\
				   stack_type_ptr[stack_type_index] = Pop(s);\
				}\
			     }
			           
#define MultiPushM(s, d)     {  STACK_TYPE *stack_type_ptr = (STACK_TYPE*)&d; int stack_type_index;\
				for(stack_type_index = 0;\
				    stack_type_index < (sizeof(d) + sizeof(STACK_TYPE) - 1)/sizeof(STACK_TYPE);\
				    stack_type_index++) {\
				   PushM(s, stack_type_ptr[stack_type_index]);\
				}\
			     }
#define MultiPopM(s, d)      {  STACK_TYPE *stack_type_ptr = (STACK_TYPE*)&d; int stack_type_index;\
				for(stack_type_index =  ((sizeof(d) + sizeof(STACK_TYPE) - 1)/sizeof(STACK_TYPE)) - 1;\
				    stack_type_index >= 0;\
				    stack_type_index--) {\
				   stack_type_ptr[stack_type_index] = PopM(s);\
				}\
			     }
			           
   
#undef _STACK_NO_PROTO
#undef _STACK_P

#ifdef __cplusplus
}
#endif

#endif  /* _STACK_H  */

