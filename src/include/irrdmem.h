#ifndef _IRRDMEM_H_FILE_
#define _IRRDMEM_H_FILE_

#include <stdlib.h>
#include <errno.h>
/**************  Use Prototype definitions?  ***************/
#ifdef  _NO_PROTO
#define _IRRDMEM_NO_PROTO
#else
#if    !(defined(__STDC__) && __STDC__) \
    && !defined(__cplusplus) && !defined(c_plusplus) \
    && !defined(FUNCPROTO) && !defined(XTFUNCPROTO) && !defined(XMFUNCPROTO)
#define _IRRDMEM_NO_PROTO
#endif /* __STDC__ */
#endif  /*  _NO_PROTO  */

#ifdef _IRRDMEM_NO_PROTO
#define _IRRDMEM_P(ARGS) ()
#else
#define _IRRDMEM_P(ARGS) ARGS
#endif

/*************  Define Abstract data type   ****************/
#ifndef _IRRDMEM_NO_PROTO
typedef void *DATA_PTR;
#else
typedef char *DATA_PTR;
#endif

/************  Function Declarations        ****************/
DATA_PTR irrd_malloc(unsigned size);
void irrd_free(DATA_PTR ptr);


#endif  /*  _IRRDMEM_H_FILE_  */

