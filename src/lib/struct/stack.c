/*-----------------------------------------------------------
 *  Name: 	stack.c
 *  Created:	Fri Aug 26 18:14:55 1994
 *  Author: 	Jonathan DeKock   <dekock@winter>
 *  DESCR:  	stack functions
 */

#include <stdio.h>
#include <stack.h>
#include <sys/types.h>
#include <defs.h>

/****************  Global Variables  *********************/
const char *STACK_errlist[] = {
   "unknown error",
   "memory error",
   "stack full",
   "stack empty",
   "invalid argument"
};

static STACK_ErrorProc STACK_Handler = (STACK_ErrorProc) STACK_DefaultHandler;

/*-----------------------------------------------------------
 *  Name:       STACK_Create()
 *  Created:	Fri Aug 26 19:02:21 1994
 *  Author: 	Jonathan DeKock   <dekock@winter>
 *  DESCR:  	creates a new stack
 */
STACK* STACK_Create(unsigned size)
{
   STACK* stack;
   
#ifdef STACK_DEBUG
   if ((int) size <= 0) {
      if (STACK_Handler) { (STACK_Handler)(NULL, STACK_BadArgument, "Create_STACK()"); }
      return(NULL);
   }
#endif
   
   stack = New(STACK);
   if (!stack) {
      if (STACK_Handler) { (STACK_Handler)(NULL, STACK_MemoryErr, "Create_STACK()"); }
      return(NULL);
   }

   stack->size = size;
   stack->top  = 0;
   stack->attr = (enum STACK_ATTR) 0;
   
   /*  Allocate the stack array  */
   if (!(stack->array = (STACK_TYPE*)NewArray(STACK_TYPE, stack->size))) {
      if (STACK_Handler) { (STACK_Handler)(NULL, STACK_MemoryErr, "Create_STACK()"); }
      Delete(stack);
      return(NULL);
   }

#ifdef STACK_DEBUG
   printf("STACK: 0x%.8x Create() {%u/%u}\n", stack, stack->top, stack->size);
#endif
   
   return(stack);
}

/*-----------------------------------------------------------
 *  Name: 	STACK_Destroy()
 *  Created:	Fri Aug 26 20:08:53 1994
 *  Author: 	Jonathan DeKock   <dekock@winter>
 *  DESCR:  	destroys a stack
 */
void STACK_Destroy(STACK* stack)
{
#ifdef STACK_DEBUG
   /*  Verify that it gets a stack  */
   if (!stack) {
      if (STACK_Handler) { (STACK_Handler)(stack, STACK_BadArgument, "Destroy_STACK()"); }
      return;
   }
   if (stack->attr & STACK_REPORT)
      printf("STACK: 0x%.8x Destroy() {%u/%u}\n", stack, stack->top, stack->size);
#endif

   Delete(stack->array);
   Delete(stack);
}

/*-----------------------------------------------------------
 *  Name: 	STACK_Push()
 *  Created:	Fri Aug 26 20:18:04 1994
 *  Author: 	Jonathan DeKock   <dekock@winter>
 *  DESCR:  	pushes an element onto the stack
 */
void STACK_Push(STACK* stack, STACK_TYPE data)
{
#ifdef STACK_DEBUG
   if (!stack) {
      if (STACK_Handler) { (STACK_Handler)(stack, STACK_BadArgument, "Push()"); }
      return;
   }
#endif
   
   /*  Check the stack limit */
   if (stack->top == stack->size) {
      if (!(stack->attr & STACK_DYNAMIC)) {
	 if (STACK_Handler) { (STACK_Handler)(stack, STACK_Full, "Push()"); }
	 return;
      }
      else {  /* Expand if Dynamic  */
	 int tmp = (stack->size > 100) ? stack->size + 100 : stack->size*2;
	 STACK_TYPE *new_stack;
	 new_stack = ReallocateArray(stack->array, STACK_TYPE, tmp);
	 if (!(new_stack)) {
	    if (STACK_Handler) { (STACK_Handler)(stack, STACK_MemoryErr, "Push()"); }
	    return;
	 }
	 stack->size = tmp; 
	 stack->array = new_stack;
#ifdef STACK_DEBUG
	 if (stack->attr & STACK_REPORT)
	    printf("STACK: 0x%.8x Resized to %u elements\n", stack, tmp);
#endif
      }
   }

   /* Do the "real" push */
   stack->array[stack->top++] = data;

#ifdef STACK_DEBUG
   if (stack->attr & STACK_REPORT)
      printf("STACK: 0x%.8x Push(%u) {%u/%u}\n", stack, data, stack->top, stack->size);
#endif
}

/*-----------------------------------------------------------
 *  Name: 	STACK_Pop()
 *  Created:	Fri Aug 26 21:18:57 1994
 *  Author: 	Jonathan DeKock   <dekock@winter>
 *  DESCR:  	pops an element off a stack.
 */
STACK_TYPE STACK_Pop(STACK* stack)
{
#ifdef STACK_DEBUG
   if (!stack) {
      if (STACK_Handler) { (STACK_Handler)(stack, STACK_BadArgument, "Pop()"); }
      return(0);
   }
#endif
   
   if (stack->top == 0) {
      if (STACK_Handler) { (STACK_Handler)(stack, STACK_Empty, "Pop()"); }
      return(0);
   }

#ifdef STACK_DEBUG
   if (stack->attr & STACK_REPORT)
      printf("STACK: 0x%.8x Pop() = %u {%u/%u}\n", stack, stack->array[stack->top-1],
	     stack->top-1, stack->size);
#endif
   
   return(stack->array[--stack->top]);
}

/*-----------------------------------------------------------
 *  Name: 	STACK_SetHandler()
 *  Created:	Sat Aug 27 21:41:08 1994
 *  Author: 	Jonathan DeKock   <dekock@winter>
 *  DESCR:  	sets the error handler for stacks
 */
STACK_ErrorProc STACK_SetHandler(STACK_ErrorProc fn, char *name)
{
   STACK_ErrorProc tmp;
   tmp = STACK_Handler;
   STACK_Handler = fn;
#ifdef STACK_DEBUG
   if (name) printf("STACK: Handler changed to %s = 0x%.8x.\n", name, fn);
   else      printf("STACK: Handler changed to 0x%.8x.\n", fn);
#endif
   return(tmp);
}

/*-----------------------------------------------------------
 *  Name: 	STACK_DefaultHandler()
 *  Created:	Sat Aug 27 21:45:08 1994
 *  Author: 	Jonathan DeKock   <dekock@winter>
 *  DESCR:  	default handler for stack errors
 */
void STACK_DefaultHandler(STACK* stack, enum STACK_ERROR num, char *name)
{
#ifdef STACK_DEBUG
   FILE *out = stdout;
#else
   FILE *out = stderr;
#endif
   fflush(stdout);
   fflush(stderr);
   fprintf(out, "STACK: 0x%.8x Error in function %s. \"%s\" {%u/%u}\n", (u_int)stack, name,
	   STACK_errlist[num], (stack) ? (u_int)stack->top : 0, (stack) ? (u_int)stack->size : 0);
   fflush(out);
}

/*-----------------------------------------------------------
 *  Name: 	STACK_Print()
 *  Created:	Sun Aug 28 20:11:29 1994
 *  Author: 	Jonathan DeKock   <dekock@winter>
 *  DESCR:  	prints a stack
 */
void STACK_Print(STACK* stack, STACK_PrintProc print)
{
   int i;

#ifdef STACK_DEBUG
   if (!stack) {
      if (STACK_Handler) { (STACK_Handler)(stack, STACK_BadArgument, "Print_STACK()"); }
      return;
   }
#endif

   if (!print) print = (STACK_PrintProc) STACK_DefaultPrintFn;
   
   printf("STACK: 0x%.8x Print(0x%.8x) {%u/%u}\n", (u_int)stack, (u_int)print, (u_int)stack->top, (u_int)stack->size);
   for (i = stack->top - 1; i >= 0; i--) {
      (print)(i, stack->array[i]);
   }

   fflush(stdout);
}

/*-----------------------------------------------------------
 *  Name: 	STACK_DefaultPrintFn()
 *  Created:	Tue Sep 13 18:56:13 1994
 *  Author: 	Jonathan DeKock   <dekock@winter>
 *  DESCR:  	default printer for stack infomation
 */
void STACK_DefaultPrintFn(unsigned num, STACK_TYPE data)
{
   printf("%3u: %d\n", num, (u_int)data);
}
