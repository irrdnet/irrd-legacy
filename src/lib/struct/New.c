/*-----------------------------------------------------------
 *  Name: 	new.c
 *  Created:	Sat Aug 20 17:52:25 1994
 *  Author: 	Jonathan DeKock   <dekock@winter>
 *  DESCR:  	memory allocation and de-allocation routines.
 */

#include <New.h>

#include <stdio.h>
#include <string.h>
#include <memory.h>
#include <sys/types.h>
#include <defs.h>

#ifdef __cplusplus
#include <new.h>
#endif


/************************  GLOBAL VARIABLES  *******************************/
static        New_ErrorProc        New_Handler =        (New_ErrorProc)        New_DefaultHandler;
static Reallocate_ErrorProc Reallocate_Handler = (Reallocate_ErrorProc) Reallocate_DefaultHandler;
static     Delete_ErrorProc     Delete_Handler =     (Delete_ErrorProc)     Delete_DefaultHandler;

/*-----------------------------------------------------------
 *  Name: 	NewMemory()
 *  Created:	Sat Aug 20 17:43:56 1994
 *  Author: 	Jonathan DeKock   <dekock@winter>
 *  DESCR:  	Allocates memory from the free store.
 */
DATA_PTR NewMemory(unsigned size, char *name, char *file, int line)
{
   DATA_PTR data;
   
#ifdef NEW_DEBUG
   if ((int) size <= 0) {  /*  Bad Size, Zero or way to huge */
      errno = EDOM;
      if (New_Handler) { (New_Handler)(size, name, file, line); }
      return(NULL);
   }
#endif

   /*  Actual Allocation  */
#ifndef __cplusplus
   data = (char *)malloc(size);                       
#else
   data = new char[size];
#endif
   
   if (!data) {                               
      if (New_Handler) { (New_Handler)(size, name, file, line); }                                       
      return(NULL);                
   }

#ifdef NEW_DEBUG
   if (name) printf("New: %3u bytes (%s) allocated at 0x%.8x. (%s, %d)\n", size, name, data, file, line);
   else      printf("New: %3u bytes allocated at 0x%.8x.\n", size, data);
#endif

   memset((char*)data, '\0', size);
   return(data);                  
}


/*-----------------------------------------------------------
 *  Name: 	RealloateMemory()
 *  Created:	Sat Sep  3 20:38:04 1994
 *  Author: 	Jonathan DeKock   <dekock@winter>
 *  DESCR:  	reallocates ptr to size specified
 */
DATA_PTR ReallocateMemory(DATA_PTR ptr, unsigned size, char *ptr_name, char *name, char *file, int line)
{
   DATA_PTR data;
#ifdef NEW_DEBUG
   if ((int) size <= 0) {  /*  Bad Size  */
      errno = EINVAL;
      if (Reallocate_Handler) { (Reallocate_Handler)(ptr, size, ptr_name, name, file, line); }
      return(NULL);
   }
#endif
   
   /*  Actual Reallocation  */
   data = (char *)realloc(ptr, size);

   if (!data) {                               
      if (Reallocate_Handler) { (Reallocate_Handler)(ptr, size, ptr_name, name, file, line); }
      return(NULL);                
   }

#ifdef NEW_DEBUG
   if (name) printf("Reallocate: %3u bytes (%s) reallocated from 0x%.8x (%s) to 0x%.8x. (%s, %d)\n",
		    size, name, ptr, ptr_name, data, file, line);
   else      printf("Reallocate: %3u bytes reallocated from 0x%.8x to 0x%.8x.\n", size, ptr, data);
#endif

   return(data);                  
}


/*-----------------------------------------------------------
 *  Name: 	DeleteMemory()
 *  Created:	Sat Aug 20 18:26:12 1994
 *  Author: 	Jonathan DeKock   <dekock@winter>
 *  DESCR:  	de-allocates memory
 */
int DeleteMemory(DATA_PTR ptr, char *name, char *file, int line)
{
#if defined(__cplusplus)
   delete ptr;

#elif  !defined(FREE_RETURNS_SUCCESS)
   free(ptr);

#else
   if (!(free(ptr))) {
      if (Delete_Handler) { (Delete_Handler)(ptr, name, file, line); }
      return(0);
   }
#endif /* FREE_RETURNS_SUCCESS */
   
#ifdef NEW_DEBUG
   if (name) printf("Delete: 0x%.8x (%s) freed. (%s, %d)\n", ptr, name, file, line);
   else      printf("Delete: 0x%.8x freed.\n", ptr);
#endif
   
   return(1);
}


int
FDelete (DATA_PTR ptr)
{
    return (DeleteMemory (ptr, NULL, NULL, 0));
}


/*-----------------------------------------------------------
 *  Name: 	Destroy()
 *  Created:	Thu Sep 15 01:10:27 1994
 *  Author: 	Jonathan DeKock   <dekock@winter>
 *  DESCR:  	interface to Delete() for auto-destruction
 *              installable on Linked Lists, and Hash Tables
 */
void Destroy(DATA_PTR ptr)
{
   DeleteMemory(ptr, NULL, NULL, 0);
}


/*-----------------------------------------------------------
 *  Name: 	New_SetHandler()
 *  Created:	Sat Aug 20 18:56:36 1994
 *  Author: 	Jonathan DeKock   <dekock@winter>
 *  DESCR:  	sets the handler for New errors
 */
New_ErrorProc New_SetHandler(New_ErrorProc ptr, char *name, char *file, int line)
{
   New_ErrorProc tmp = New_Handler;
   New_Handler = ptr;
#ifdef NEW_DEBUG
   if (name) printf("New: Handler changed to %s = 0x%.8x. (%s, %d)\n", name, ptr, file, line);
   else      printf("New: Handler changed to 0x%.8x.\n", ptr);
#endif
   return(tmp);
}

/*-----------------------------------------------------------
 *  Name: 	Reallocate_SetHandler()
 *  Created:	Sat Aug 20 18:56:36 1994
 *  Author: 	Jonathan DeKock   <dekock@winter>
 *  DESCR:  	sets the handler for New errors
 */
Reallocate_ErrorProc Reallocate_SetHandler(Reallocate_ErrorProc ptr, char *name, char *file, int line)
{
   Reallocate_ErrorProc tmp = Reallocate_Handler;
   Reallocate_Handler = ptr;
#ifdef NEW_DEBUG
   if (name) printf("Reallocate: Handler changed to %s = 0x%.8x. (%s, %d)\n", name, ptr, file, line);
   else      printf("Reallocate: Handler changed to 0x%.8x.\n", ptr);
#endif
   return(tmp);
}


/*-----------------------------------------------------------
 *  Name: 	Delete_SetHandler()
 *  Created:	Sat Aug 20 18:59:03 1994
 *  Author: 	Jonathan DeKock   <dekock@winter>
 *  DESCR:  	sets the handler for delete errors
 */
Delete_ErrorProc Delete_SetHandler(Delete_ErrorProc ptr, char *name, char *file, int line)
{
   Delete_ErrorProc tmp = Delete_Handler;
   Delete_Handler = ptr;
#ifdef NEW_DEBUG
   if (name) printf("Delete: Handler changed to %s = 0x%.8x. (%s, %d)\n", name, ptr, file, line);
   else      printf("Delete: Handler changed to 0x%.8x.\n", ptr);
#endif
   return(tmp);
}

/*-----------------------------------------------------------
 *  Name: 	New_DefaultHandler()
 *  Created:	Sat Aug 20 18:40:52 1994
 *  Author: 	Jonathan DeKock   <dekock@winter>
 *  DESCR:  	default handler for New errors
 */
void New_DefaultHandler(unsigned size, char *name, char *file, int line)
{
#ifdef NEW_DEBUG
   FILE *out = stdout;
#else
   FILE *out = stderr;
#endif
   fflush(stdout);
   fflush(stderr);
   if (name) fprintf(out, "New: Error allocating %u bytes (%s). \"%s\" (%s, %d)\n",
		     size, name, strerror(errno), file, line);
   else      fprintf(out, "New: Error allocating %u bytes. \"%s\"\n", size, strerror(errno));
   fflush(out);
}

/*-----------------------------------------------------------
 *  Name: 	Reallocate_DefaultHandler()
 *  Created:	Sat Aug 20 18:42:52 1994
 *  Author: 	Jonathan DeKock   <dekock@winter>
 *  DESCR:  	default handler for New errors
 */
void Reallocate_DefaultHandler(DATA_PTR ptr, unsigned size, char *ptr_name, char *name, char *file, int line)
{
#ifdef NEW_DEBUG
   FILE *out = stdout;
#else
   FILE *out = stderr;
#endif
   fflush(stdout);
   fflush(stderr);
   if (name) fprintf(out, "Reallocate: Error reallocating 0x%.8x (%s) to %d bytes (%s). \"%s\" (%s, %d)\n",
		     (u_int)ptr, ptr_name, size, name, strerror(errno), file, line);
   else      fprintf(out, "Reallocate: Error reallocating 0x%.8x to %d bytes. \"%s\"\n",
		     (u_int)ptr, size, strerror(errno));
   fflush(out);
}


/*-----------------------------------------------------------
 *  Name: 	Delete_DefaultHandler()
 *  Created:	Sat Aug 20 18:45:44 1994
 *  Author: 	Jonathan DeKock   <dekock@winter>
 *  DESCR:  	default handler for Delete errors
 */
void Delete_DefaultHandler(DATA_PTR ptr, char *name, char *file, int line)
{
#ifdef NEW_DEBUG
   FILE *out = stdout;
#else
   FILE *out = stderr;
#endif
   fflush(stdout);
   fflush(stderr);
   if (name) fprintf(out, "Delete: Error freeing 0x%.8x (%s). \"%s\" (%s, %d)\n",
		     (u_int)ptr, name, strerror(errno), file, line);
   else      fprintf(out, "Delete: Error freeing 0x%.8x. \"%s\"\n", (u_int)ptr, strerror(errno));
   fflush(out);
}


