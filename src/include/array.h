/*-----------------------------------------------------------
 *  Name: 	array.h
 *  Created:	Sat Sep  3 02:44:31 1994
 *  Author: 	Jonathan DeKock   <dekock@winter>
 *  DESCR:  	array manipulation function definitions
 */

#ifndef _ARRAY_H
#define _ARRAY_H

#ifdef __cplusplus
extern "C" {
#endif

#include <New.h>
#include <stack.h>

/**************  Debugging turned on or off  *************/
#ifdef STRUCT_DEBUG
#define ARRAY_DEBUG
#endif
   
/**************  Use Prototype definitions?  *************/
#ifdef  _NO_PROTO
#define _ARRAY_NO_PROTO
#else
#if    !(defined(__STDC__) && __STDC__) \
   && !defined(__cplusplus) && !defined(c_plusplus) \
   && !defined(FUNCPROTO) && !defined(XTFUNCPROTO) && !defined(XMFUNCPROTO)
#define _ARRAY_NO_PROTO
#endif /* __STDC__ */
#endif  /*  _NO_PROTO  */
       
#ifdef _ARRAY_NO_PROTO
#define _ARRAY_P(ARGS) ()
#else
#define _ARRAY_P(ARGS) ARGS
#endif

/*********************  Process Definitions  ***********************/
typedef int  (*ARRAY_CompareProc)  _ARRAY_P((DATA_PTR, DATA_PTR));
typedef int  (*ARRAY_SearchProc)   _ARRAY_P((DATA_PTR, DATA_PTR));
typedef int  (*ARRAY_FindProc)     _ARRAY_P((DATA_PTR, DATA_PTR));

/*********************  Function Definitions ***********************/
/* First arg. is really supposed to be DATA_PTR*, and last is the function */
extern DATA_PTR ARRAY_BinarySearch _ARRAY_P((DATA_PTR, unsigned, DATA_PTR, DATA_PTR)); 
extern DATA_PTR ARRAY_Find         _ARRAY_P((DATA_PTR, unsigned, DATA_PTR, DATA_PTR));
   
extern void     ARRAY_Sort         _ARRAY_P((DATA_PTR, unsigned, DATA_PTR));
extern void     ARRAY_BubbleSort   _ARRAY_P((DATA_PTR, unsigned, DATA_PTR));
extern void     ARRAY_QuickSort    _ARRAY_P((DATA_PTR, unsigned, DATA_PTR));
extern void     ARRAY_MergeSort    _ARRAY_P((DATA_PTR, unsigned, DATA_PTR));

#undef _ARRAY_NO_PROTO
#undef _ARRAY_P
      
#ifdef __cplusplus
}
#endif

#endif  /*  _ARRAY_H  */
