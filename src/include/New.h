/*-----------------------------------------------------------
 *  Name: 	New.h
 *  Created:	Sat Aug 20 17:59:12 1994
 *  Author: 	Jonathan DeKock   <dekock@winter>
 *  DESCR:  	memory allocation header file
 */

#ifndef _NEW_H_FILE_
#define _NEW_H_FILE_

#include <stdlib.h>
#include <errno.h>

#ifdef __cplusplus
extern "C" {
#endif

/**************  Debug Turned ON or OFF      ***************/
#ifdef STRUCT_DEBUG
#define NEW_DEBUG
#endif

/**************  Use Prototype definitions?  ***************/
#ifdef  _NO_PROTO
#define _NEW_NO_PROTO
#else
#if    !(defined(__STDC__) && __STDC__) \
    && !defined(__cplusplus) && !defined(c_plusplus) \
    && !defined(FUNCPROTO) && !defined(XTFUNCPROTO) && !defined(XMFUNCPROTO)
#define _NEW_NO_PROTO
#endif /* __STDC__ */
#endif  /*  _NO_PROTO  */

#ifdef _NEW_NO_PROTO
#define _NEW_P(ARGS) ()
#else
#define _NEW_P(ARGS) ARGS
#endif
   
/*************  Define Abstract data type   ****************/
#ifndef _NEW_NO_PROTO
typedef void *DATA_PTR;
#else
typedef char *DATA_PTR;
#endif  

/*************  Function Types Declaration  ****************/
typedef void         (*New_ErrorProc) _NEW_P((          unsigned,        char*, char*, int));
typedef void  (*Reallocate_ErrorProc) _NEW_P((DATA_PTR, unsigned, char*, char*, char*, int));
typedef void      (*Delete_ErrorProc) _NEW_P((DATA_PTR,           char*,        char*, int));
/* Paramaters: address, size, address name, size name, file name, line number */


/************  Global Variables             ****************/
/*extern char *sys_errlist[];  error list text (errno.h) */

/************  Function Declarations        ****************/
extern DATA_PTR             NewMemory                 _NEW_P((unsigned, char*, char*, int));
extern DATA_PTR             ReallocateMemory          _NEW_P((DATA_PTR, unsigned, char*, char*, char*, int));
extern int                  DeleteMemory              _NEW_P((DATA_PTR, char*, char*, int));
extern void                 Destroy                   _NEW_P((DATA_PTR));
    
extern        New_ErrorProc New_SetHandler            _NEW_P((New_ErrorProc, char*, char*, int));
extern Reallocate_ErrorProc Reallocate_SetHandler     _NEW_P((Reallocate_ErrorProc, char*, char*, int));
extern     Delete_ErrorProc Delete_SetHandler         _NEW_P((Delete_ErrorProc, char*, char*, int));

extern void                 New_DefaultHandler        _NEW_P((unsigned, char*, char*, int));
extern void                 Reallocate_DefaultHandler _NEW_P((DATA_PTR, unsigned, char*, char*, char*, int));
extern void                 Delete_DefaultHandler     _NEW_P((DATA_PTR, char*, char*, int));

/************  Macro Declarations           ****************/
#ifndef NEW_DEBUG  

/* New */
#define New(a)         (a*)NewMemory(sizeof(a),   NULL, NULL, 0)
#define NewArray(a, x) (a*)NewMemory(sizeof(a)*x, NULL, NULL, 0)
#define NewSize(a)         NewMemory(a,           NULL, NULL, 0)

/* Reallocate */
#define Reallocate(p, a)         (a*)ReallocateMemory(p, sizeof(a),   NULL, NULL, NULL, 0)
#define ReallocateArray(p, a, x) (a*)ReallocateMemory(p, sizeof(a)*(x), NULL, NULL, NULL, 0)
#define ReallocateSize(p, a)         ReallocateMemory(p, a,           NULL, NULL, NULL, 0)

/* Delete */
#define Delete(ptr) DeleteMemory(ptr, NULL, NULL, 0)
extern int FDelete (DATA_PTR);

/* Handler Control */
#define Set_New_Handler(ptr)               New_SetHandler(       (New_ErrorProc)ptr, NULL, NULL, 0)
#define Set_Reallocate_Handler(ptr) Reallocate_SetHandler((Reallocate_ErrorProc)ptr, NULL, NULL, 0)
#define Set_Delete_Handler(ptr)         Delete_SetHandler(    (Delete_ErrorProc)ptr, NULL, NULL, 0)
    
#else   
    
/* New */
#define New(a)           (a*)NewMemory(sizeof(a),   #a,                     __FILE__, __LINE__)
#define NewArray(a, x)   (a*)NewMemory(sizeof(a)*(x), #x " elt array of " #a "'s", __FILE__, __LINE__)
#define NewSize(a)           NewMemory(a,           "unspecified object",   __FILE__, __LINE__)
    
/* Reallocate */
#define Reallocate(p, a)         (a*)ReallocateMemory(p, sizeof(a),   #p, #a,                     __FILE__, __LINE__)
#define ReallocateArray(p, a, x) (a*)ReallocateMemory(p, sizeof(a)*(x), #p, #x " elt array of " #a "'s", __FILE__, __LINE__)
#define ReallocateSize(p, a)         ReallocateMemory(p, a,           #p, "unspecified object",   __FILE__, __LINE__)

/* Delete */
#define Delete(ptr) DeleteMemory(ptr, #ptr, __FILE__, __LINE__)

/* Handler Control */
#define Set_New_Handler(ptr)               New_SetHandler(       (New_ErrorProc)ptr, #ptr, __FILE__, __LINE__)
#define Set_Reallocate_Handler(ptr) Reallocate_SetHandler((Reallocate_ErrorProc)ptr, #ptr, __FILE__, __LINE__)
#define Set_Delete_Handler(ptr)         Delete_SetHandler(    (Delete_ErrorProc)ptr, #ptr, __FILE__, __LINE__)

#endif  

#undef _NEW_NO_PROTO
#undef _NEW_P

#ifdef __cplusplus
}
#endif

#endif  /*  _NEW_H_FILE_  */



