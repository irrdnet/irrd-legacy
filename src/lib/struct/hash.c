/*-----------------------------------------------------------
 *  Name: 	hash.c
 *  Created:	Thu Sep  8 02:47:41 1994
 *  Author: 	Jonathan DeKock   <dekock@winter>
 *  DESCR:  	hash table functions
 */

#include <New.h>
#include <hash.h>
/*#include <stdio.h>*/
#include <stdarg.h>
#include <string.h>
#include <defs.h>
#include <linked_list.h>
#include <sys/types.h>

/******************* Global Variables **********************/
static HASH_ErrorProc HASH_Handler = (HASH_ErrorProc) HASH_DefaultHandler;

const char *HASH_errlist[] = {
   "unknown error",
   "memory error",
   "hash table corrupted",
   "invalid hash index",
   "invalid argument",
   "no member matching key",
};

/******************* Internal Macros ***********************/

/* Intrusive Macros for Next and Key */
#define NextP(h, d) *(DATA_PTR*)((char*)d + h->next_offset)
#define KeyP(h, d)  ((DATA_PTR) ((h->attr & HASH_EmbeddedKey) ? \
				 ((char*)d + h->key_offset) : \
				 *(DATA_PTR*)((char*)d + h->key_offset)))


/*-----------------------------------------------------------
 *  Name: 	HMacro_SetAttr()
 *  Created:	Mon Sep 26 19:44:16 1994
 *  Author: 	Jonathan DeKock   <dekock@winter>
 *  DESCR:  	sets a given attribute of list
 */
#define HMacro_SetAttr(h, attr, val, ptr) \
   switch (attr) {\
    case HASH_Intrusive:\
      val = va_arg(ap, int);\
      h->attr = (enum HASH_ATTR)((val) ? h->attr | HASH_Intrusive : h->attr & ~HASH_Intrusive);\
      break;\
    case HASH_NonIntrusive:\
      val = va_arg(ap, int);\
      h->attr = (enum HASH_ATTR)((!val) ? h->attr | HASH_Intrusive : h->attr & ~HASH_Intrusive);\
      break;\
    case HASH_EmbeddedKey:\
      val = va_arg(ap, int);\
      h->attr = (enum HASH_ATTR)((val) ? h->attr | HASH_EmbeddedKey : h->attr & ~HASH_EmbeddedKey);\
      break;\
    case HASH_ReportChange:\
      val = va_arg(ap, int);\
      h->attr = (enum HASH_ATTR)((val) ? h->attr | HASH_ReportChange : h->attr & ~HASH_ReportChange);\
      break;\
    case HASH_ReportAccess:\
      val = va_arg(ap, int);\
      h->attr = (enum HASH_ATTR)((val) ? h->attr | HASH_ReportAccess : h->attr & ~HASH_ReportAccess);\
      break;\
    case HASH_NextOffset:\
      val = va_arg(ap, int);\
      h->next_offset = val;\
      h->attr = (enum HASH_ATTR)(h->attr | HASH_Intrusive);\
      break;\
    case HASH_KeyOffset:\
      val = va_arg(ap, int);\
      h->key_offset = val;\
      break;\
    case HASH_HashFunction:\
      ptr = va_arg(ap, DATA_PTR);\
      h->hash = (HASH_HashProc)ptr;\
      break;\
    case HASH_LookupFunction:\
      ptr = va_arg(ap, DATA_PTR);\
      h->lookup = (HASH_LookupProc)ptr;\
      break;\
    case HASH_DestroyFunction:\
      ptr = va_arg(ap, DATA_PTR);\
      h->destroy = (HASH_DestroyProc)ptr;\
      break;\
    default:\
      attr = 0;\
      break;\
   }

/******************* Function Definitions ******************/

/*-----------------------------------------------------------
 *  Name: 	HASH_Create()
 *  Created:	Thu Sep  8 02:53:32 1994
 *  Author: 	Jonathan DeKock   <dekock@winter>
 *  DESCR:  	creates a hash table
 */
HASH_TABLE *HASH_Create(unsigned size, int first, ...)
{
   va_list ap;
   enum HASH_ATTR attr;
   int  val;
   DATA_PTR ptr;
   HASH_TABLE *h;

#ifdef HASH_DEBUG
   if (!size) {
      if (HASH_Handler) { (HASH_Handler)(NULL, HASH_BadArgument, "HASH_Create()"); }
      return(NULL);
   }
#endif
   
   if (!(h = New(HASH_TABLE))) {
      if (HASH_Handler) { (HASH_Handler)(h, HASH_MemoryErr, "HASH_Create()"); }
      return(NULL);
   }

   if (!(h->array.data = NewArray(DATA_PTR, size))) {
      if (HASH_Handler) { (HASH_Handler)(h, HASH_MemoryErr, "HASH_Create()"); }
      Delete(h);
      return(NULL);
   }

   h->size = size;
   h->count = 0;
   h->next_offset = sizeof(DATA_PTR);
   h->key_offset = 0;
   h->attr = (enum HASH_ATTR) 0;
   h->hash = (HASH_HashProc)HASH_DefaultHashFunction;
   h->lookup = (HASH_LookupProc)HASH_DefaultLookupFunction;
   h->destroy = NULL;

   va_start(ap, first);
   for (attr = (enum HASH_ATTR)first; attr; attr = va_arg(ap, enum HASH_ATTR)) {
      HMacro_SetAttr(h, attr, val, ptr);
      if (!attr) {
	 if (HASH_Handler) { (HASH_Handler)(h, HASH_BadArgument, "HASH_Create()"); }
	 Delete(h->array.data);
	 Delete(h);
	 va_end(ap);
	 return(NULL);
      }
   }
   va_end(ap);
   
#ifdef HASH_DEBUG
   if (h->attr & HASH_ReportChange) {
      printf("HASH TABLE: 0x%.8x Create() => %s, %s, %s\n", h,
	     (h->attr & HASH_Intrusive) ? "Intrusive" : "Container",
	     (h->attr & HASH_ReportChange) ? "Report Change" : "No Change Report",
	     (h->attr & HASH_ReportAccess) ? "Report Access" : "No Access Report");
      printf("HASH TABLE: 0x%.8x static size = %u\n", h, h->size);
      if (h->attr & HASH_Intrusive) {
	 printf("HASH TABLE: 0x%.8x key offset = %u, next offset = %u\n",
		h, h->key_offset, h->next_offset);
      }
      else {
	 printf("HASH TABLE: 0x%.8x key offset = %u\n", h, h->key_offset);
      }
      printf("HASH TABLE: 0x%.8x functions: hash = 0x%.8x, lookup = 0x%.8x, destroy = 0x%.8x\n",
	     h, h->hash, h->lookup, h->destroy);
   }
#endif

#ifdef _HASH_INTERNAL_DEBUG
   HASH_Verify(h);
#endif

   return(h);
}


/*-----------------------------------------------------------
 *  Name: 	HASH_SetAttributes()
 *  Created:	Mon Sep 26 19:51:27 1994
 *  Author: 	Jonathan DeKock   <dekock@winter>
 *  DESCR:  	sets the attributes of the hash table
 */
void HASH_SetAttributes(HASH_TABLE *h, int first, ...)
{
   va_list ap;
   enum HASH_ATTR attr;
   int val;
   DATA_PTR ptr;

#ifdef HASH_DEBUG
   if (!h) {
      if (HASH_Handler) { (HASH_Handler)(h, HASH_BadArgument, "HASH_SetAttributes()"); }
      return;
   }
#endif
   
   va_start(ap, first);
   for (attr = (enum HASH_ATTR)first; attr; attr = va_arg(ap, enum HASH_ATTR)) {
      HMacro_SetAttr(h, attr, val, ptr);
      if (!attr) {
	 if (HASH_Handler) { (HASH_Handler)(h, HASH_BadArgument, "HASH_SetAttributes()"); }
	 va_end(ap);
	 return;
      }
#ifdef HASH_DEBUG
      if (h->count > 0) {
	 if (attr & (HASH_EmbeddedKey | HASH_Intrusive | HASH_NextOffset | HASH_KeyOffset
		     | HASH_HashFunction | HASH_LookupFunction)) {
	    if (HASH_Handler) { (HASH_Handler)(h, HASH_BadArgument, "HASH_SetAttributes()"); }
	    return;
	 }
      }
#endif
   }
   va_end(ap);
   
#ifdef HASH_DEBUG
   if (h->attr & HASH_ReportChange) {
      printf("HASH TABLE: 0x%.8x SetAttributes() => %s, %s, %s\n", h,
	     (h->attr & HASH_Intrusive) ? "Intrusive" : "Container",
	     (h->attr & HASH_ReportChange) ? "Report Change" : "No Change Report",
	     (h->attr & HASH_ReportAccess) ? "Report Access" : "No Access Report");
      printf("HASH TABLE: 0x%.8x size = %u\n", h, h->size);
      if (h->attr & HASH_Intrusive) {
	 printf("HASH TABLE: 0x%.8x key offset = %u, next offset = %u\n",
		h, h->key_offset, h->next_offset);
      }
      else {
	 printf("HASH TABLE: 0x%.8x key offset = %u\n", h, h->key_offset);
      }
      printf("HASH TABLE 0x%.8x functions: hash = 0x%.8x, lookup = 0x%.8x, destroy = 0x%.8x\n",
	     h, h->hash, h->lookup, h->destroy);
   }
#endif

#ifdef _HASH_INTERNAL_DEBUG
   HASH_Verify(h);
#endif
}

 /*  Name: 	HASH_ClearFn()
 *  Created:	Thu Sep  8 14:56:40 1994
 *  Author: 	Jonathan DeKock   <dekock@winter>
 *  DESCR:  	clears all items from a hash table
 */
void HASH_ClearFn(HASH_TABLE* h, HASH_DestroyProc destroy)
{
   u_int index;
#ifdef HASH_DEBUG
   if (!h) {
      if (HASH_Handler) { (HASH_Handler)(h, HASH_BadArgument, "HASH_Clear()"); }
      return;
   }
#endif
   if (h->attr & HASH_Intrusive) {
      DATA_PTR data;
      for (index = 0; index < h->size; index++) { /* Run thru all the indicies */
	 while ((data = h->array.data[index])) { /* each item on list */
	    h->array.data[index] = NextP(h, data);
#ifdef HASH_DEBUG
	    NextP(h, data) = NULL;
#endif
	    if (destroy) { (destroy)(data); }
	 }
      }
   }
   else {
      HASH_CONTAINER *cont;
      for (index = 0; index < h->size; index++) { /* Run thru all the indicies */
	 while ((cont = h->array.cont[index])) { /* each item on list */
	    h->array.cont[index] = cont->next;
	    if (destroy) { (destroy)(cont->data); }
#ifdef HASH_DEBUG
	    cont->next = NULL;
	    cont->data = NULL;
#endif
	    Delete(cont);
	 }
      }
   }
   h->count = 0;
#ifdef HASH_DEBUG
   if (h->attr & HASH_ReportChange) {
      printf("HASH TABLE: 0x%.8x Clear(0x%.8x)\n", h, destroy);
   }
#endif

#ifdef _HASH_INTERNAL_DEBUG
   HASH_Verify(h);
#endif
}

/*-----------------------------------------------------------
 *  Name: 	HASH_DestroyFn()
 *  Created:	Thu Sep  8 15:10:10 1994
 *  Author: 	Jonathan DeKock   <dekock@winter>
 *  DESCR:  	completely destroys a hash table
 */
void HASH_DestroyFn(HASH_TABLE* h, HASH_DestroyProc destroy)
{
#ifdef HASH_DEBUG
   if (!h) {
      if (HASH_Handler) { (HASH_Handler)(h, HASH_BadArgument, "HASH_Clear()"); }
      return;
   }
#endif
   HASH_ClearFn(h, destroy);
   Delete(h->array.data);
#ifdef HASH_DEBUG
   if (h->attr & HASH_ReportChange) {
      printf("HASH TABLE: 0x%.8x Destroy(0x%.8x)\n", h, destroy);
   }
#endif
   Delete(h);
}

/*-----------------------------------------------------------
 *  Name: 	HASH_Insert()
 *  Created:	Thu Sep  8 15:37:07 1994
 *  Author: 	Jonathan DeKock   <dekock@winter>
 *  DESCR:  	insert an item into a table
 */
DATA_PTR HASH_Insert(HASH_TABLE* h, DATA_PTR data)
{
   DATA_PTR key;
   unsigned index;

#ifdef HASH_DEBUG
   if (!h || !data) {
      if (HASH_Handler) { (HASH_Handler)(h, HASH_BadArgument, "HASH_Insert()"); }
      return(NULL);
   }
#endif
   
   key = KeyP(h, data);
   
#ifdef HASH_DEBUG
   if (!key) {
      if (HASH_Handler) { (HASH_Handler)(h, HASH_BadArgument, "HASH_Insert()"); }
      return(NULL);
   }
#endif

   index = (h->hash)(key, h->size);
#ifdef HASH_DEBUG
   if (index >= h->size) {
      if (HASH_Handler) { (HASH_Handler)(h, HASH_BadIndex, "HASH_Insert()"); }
      return(NULL);
   }
#endif

   if (h->attr & HASH_Intrusive) {
      NextP(h, data) = h->array.data[index];
      h->array.data[index] = data;
   }
   else {
      HASH_CONTAINER *cont = New(HASH_CONTAINER);
      if (!cont) {
	 if (HASH_Handler) { (HASH_Handler)(h, HASH_MemoryErr, "HASH_Insert()"); }
	 return(NULL);
      }
      cont->data = data;
      cont->next = h->array.cont[index];
      h->array.cont[index] = cont;
   }
   h->count++;
   
#ifdef HASH_DEBUG
   if (h->attr & HASH_ReportChange) {
      printf("HASH TABLE: 0x%.8x Insert(0x%.8x, 0x%.8x) (%u) count = %u\n",
	     h, data, key, index, h->count);
   }
#endif
   
#ifdef _HASH_INTERNAL_DEBUG
   HASH_Verify(h);
#endif
   return(data);
}
   
/*-----------------------------------------------------------
 *  Name: 	HASH_RemoveFn()
 *  Created:	Thu Sep  8 16:11:12 1994
 *  Author: 	Jonathan DeKock   <dekock@winter>
 *  DESCR:  	removes an item from a hash table given key <key>
 */
void HASH_RemoveFn(HASH_TABLE *h, DATA_PTR data, HASH_DestroyProc destroy)
{
   unsigned index;
   
#ifdef HASH_DEBUG
   if (!h || !data) {
      if (HASH_Handler) { (HASH_Handler)(h, HASH_BadArgument, "HASH_Remove()"); }
      return;
   }
#endif

   index = (h->hash)(KeyP(h, data), h->size);
#ifdef HASH_DEBUG
   if (index >= h->size) {
      if (HASH_Handler) { (HASH_Handler)(h, HASH_BadIndex, "HASH_Remove()"); }
      return;
   }
#endif

   if (h->attr & HASH_Intrusive) {
      DATA_PTR tmp, prev;
      /* Find Prev */
      for (tmp = h->array.data[index], prev = NULL; tmp; prev = tmp, tmp = NextP(h, tmp)) {
	 if (tmp == data) break;
      }
#ifdef HASH_DEBUG
      if (!tmp) {
	 if (HASH_Handler) { (HASH_Handler)(h, HASH_NoMember, "HASH_Remove()"); }
	 return;
      }
#endif
      if (prev) { NextP(h, prev) = NextP(h, data); }
      else { h->array.data[index] = NextP(h, data); }
#ifdef HASH_DEBUG
      NextP(h, data) = NULL;
#endif
      if (destroy) { (destroy)(data); }
   }
   else {
      HASH_CONTAINER *cont, *prev;
      for (cont = h->array.cont[index], prev = NULL; cont; prev = cont, cont = cont->next) {
	 if (cont->data == data) break;
      }
#ifdef HASH_DEBUG
      if (!cont) {
	 if (HASH_Handler) { (HASH_Handler)(h, HASH_NoMember, "HASH_Remove()"); }
	 return;
      }
#endif
      if (prev) { prev->next = cont->next; }
      else { h->array.cont[index] = cont->next; }
      if (destroy) { (destroy)(cont->data); }
#ifdef HASH_DEBUG
      cont->next = NULL;
      cont->data = NULL;
#endif
      Delete(cont);
   }
   h->count--;
   
#ifdef HASH_DEBUG
   if (h->attr & HASH_ReportChange) {
      printf("HASH TABLE: 0x%.8x Remove(0x%.8x, 0x%.8x) count = %u\n",
	     h, data, destroy, h->count);
   }
#endif
   
#ifdef _HASH_INTERNAL_DEBUG
   HASH_Verify(h);
#endif

}

/*-----------------------------------------------------------
 *  Name: 	HASH_RemoveByKeyFn()
 *  Created:	Thu Sep  8 16:11:12 1994
 *  Author: 	Jonathan DeKock   <dekock@winter>
 *  DESCR:  	removes an item from a hash table given key <key>
 */
void HASH_RemoveByKeyFn(HASH_TABLE *h, DATA_PTR key, HASH_DestroyProc destroy)
{
   unsigned index;
   DATA_PTR data;
   
#ifdef HASH_DEBUG
   if (!h || !key) {
      if (HASH_Handler) { (HASH_Handler)(h, HASH_BadArgument, "HASH_RemoveByKey()"); }
      return;
   }
#endif

   index = (h->hash)(key, h->size);
#ifdef HASH_DEBUG
   if (index >= h->size) {
      if (HASH_Handler) { (HASH_Handler)(h, HASH_BadIndex, "HASH_RemoveByKey()"); }
      return;
   }
#endif

   if (h->attr & HASH_Intrusive) {
      DATA_PTR prev;
      for (data = h->array.data[index], prev = NULL; data; prev = data, data = NextP(h, data)) {
	 if ((h->lookup)(KeyP(h, data), key)) break; /* found it */
      }
#ifdef HASH_DEBUG
      if (!data) {
	 if (HASH_Handler) { (HASH_Handler)(h, HASH_NoMember, "HASH_RemoveByKey()"); }
	 return;
      }
#endif
      if (prev) { NextP(h, prev) = NextP(h, data); }
      else { h->array.data[index] = NextP(h, data); }
#ifdef HASH_DEBUG
      NextP(h, data) = NULL;
#endif
      if (destroy) { (destroy)(data); }
   }
   else {
      HASH_CONTAINER *cont, *prev;
      for (cont = h->array.cont[index], prev = NULL; cont; prev = cont, cont = cont->next) {
	 if ((h->lookup)(KeyP(h, cont->data), key)) break;  /* Found it */
      }
#ifdef HASH_DEBUG
      if (!cont) {
	 if (HASH_Handler) { (HASH_Handler)(h, HASH_NoMember, "HASH_Remove()"); }
	 return;
      }
#endif
      if (!cont) /* JW 6/1/98 fix for case when item not found */
	return;
      if (prev) { prev->next = cont->next; }
      else { h->array.cont[index] = cont->next; }
      data = cont->data;
      if (destroy) { (destroy)(cont->data); }
#ifdef HASH_DEBUG
      cont->next = NULL;
      cont->data = NULL;
#endif
      Delete(cont);
   }
   h->count--;
   
#ifdef HASH_DEBUG
   if (h->attr & HASH_ReportChange) {
      printf("HASH TABLE: 0x%.8x RemoveByKey(0x%.8x, 0x%.8x) %u => 0x%.8x, count = %u\n",
	     h, key, destroy, index, data, h->count);
   }
#endif
   
#ifdef _HASH_INTERNAL_DEBUG
   HASH_Verify(h);
#endif

}

/*-----------------------------------------------------------
 *  Name: 	HASH_ReHash()
 *  Created:	Thu Sep  8 17:09:25 1994
 *  Author: 	Jonathan DeKock   <dekock@winter>
 *  DESCR:  	re-hashes an item, due to change in key.
 */
void HASH_ReHash(HASH_TABLE *h, DATA_PTR data, DATA_PTR old_key)
{
   unsigned index;
   DATA_PTR key;
   
#ifdef HASH_DEBUG
   if (!h || !data || !old_key) {
      if (HASH_Handler) { (HASH_Handler)(h, HASH_BadArgument, "HASH_ReHash()"); }
      return;
   }
#endif
   
   key = KeyP(h, data);

#ifdef HASH_DEBUG
   if (!key) {
      if (HASH_Handler) { (HASH_Handler)(h, HASH_BadArgument, "HASH_ReHash()"); }
      return;
   }
#endif

   index = (h->hash)(old_key, h->size);
#ifdef HASH_DEBUG
   if (index >= h->size) {
      if (HASH_Handler) { (HASH_Handler)(h, HASH_BadIndex, "HASH_ReHash()"); }
      return;
   }
#endif

   if (h->attr & HASH_Intrusive) {
      DATA_PTR prev, tmp;
      for (tmp = h->array.data[index], prev = NULL; tmp && tmp != data; prev = tmp, tmp = NextP(h, tmp));
#ifdef HASH_DEBUG
      if (tmp) {
	 if (HASH_Handler) { (HASH_Handler)(h, HASH_NoMember, "HASH_ReHash()"); }
	 return;
      }
#endif
      if (prev) { NextP(h, prev) = NextP(h, data); }
      else { h->array.data[index] = NextP(h, data); }
      NextP(h, data) = NULL;
      index = (h->hash)(key, h->size);
#ifdef HASH_DEBUG
      if (index >= h->size) {
	 if (HASH_Handler) { (HASH_Handler)(h, HASH_BadIndex, "HASH_ReHash()"); }
	 return;
      }
#endif
      if (h->array.data[index]) NextP(h, data) = h->array.data[index];
      h->array.data[index] = data;
   }
   else {
      HASH_CONTAINER *prev, *tmp;
      for (tmp = h->array.cont[index], prev = NULL; tmp && tmp->data != data; prev = tmp, tmp = tmp->next);
#ifdef HASH_DEBUG
      if (tmp) {
	 if (HASH_Handler) { (HASH_Handler)(h, HASH_NoMember, "HASH_Remove()"); }
	 return;
      }
#endif
      if (prev) { prev->next = tmp->next; }
      else { h->array.cont[index] = tmp->next; }
      tmp->next = NULL;
      index = (h->hash)(key, h->size);
#ifdef HASH_DEBUG
      if (index >= h->size) {
	 if (HASH_Handler) { (HASH_Handler)(h, HASH_BadIndex, "HASH_Remove()"); }
	 return;
      }
#endif
      if (h->array.cont[index]) tmp->next = h->array.cont[index];
      h->array.cont[index] = tmp;
   }

#ifdef HASH_DEBUG
   if (h->attr & HASH_ReportChange) {
      printf("HASH TABLE: 0x%.8x ReHash(0x%.8x, 0x%.8x, 0x%.8x)\n",
	     h, data, old_key, key);
   }
#endif
   
#ifdef _HASH_INTERNAL_DEBUG
   HASH_Verify(h);
#endif

}



/*-----------------------------------------------------------
 *  Name: 	HASH_Lookup()
 *  Created:	Thu Sep  8 20:23:56 1994
 *  Author: 	Jonathan DeKock   <dekock@winter>
 *  DESCR:  	looks up an item in the hash table
 */
DATA_PTR HASH_Lookup(HASH_TABLE* h, DATA_PTR key)
{
   unsigned index;
   DATA_PTR data;
#ifdef HASH_DEBUG
   if (!h || !key) {
      if (HASH_Handler) { (HASH_Handler)(h, HASH_BadArgument, "HASH_Lookup()"); }
      return(NULL);
   }
#endif
   index = (h->hash)(key, h->size);
#ifdef HASH_DEBUG
   if (index >= h->size) {
      if (HASH_Handler) { (HASH_Handler)(h, HASH_BadIndex, "HASH_Lookup()"); }
      return(NULL);
   }
#endif
   
   if (h->attr & HASH_Intrusive) {
      for (data = h->array.data[index]; data; data = NextP(h, data)) {
	 if ((h->lookup)(KeyP(h, data), key)) break;
      }
   }
   else {
      HASH_CONTAINER *cont;
      for (cont = h->array.cont[index]; cont; cont = cont->next) {
	 if ((h->lookup)(KeyP(h, cont->data), key)) break;
      }
      if (cont) data = cont->data;
      else data = NULL;
   }
#ifdef HASH_DEBUG
   if (h->attr & HASH_ReportAccess) {
      if (data) printf("HASH TABLE: 0x%.8x Lookup(0x%.8x) = 0x%.8x\n", h, key, data);
      else      printf("HASH TABLE: 0x%.8x Lookup(0x%.8x) = NULL\n", h, key);
   }
#endif

#ifdef _HASH_INTERNAL_DEBUG
   HASH_Verify(h);
#endif

   return(data);
}


/*-----------------------------------------------------------
 *  Name: 	HASH_Process()
 *  Created:	Thu Sep  8 20:33:11 1994
 *  Author: 	Jonathan DeKock   <dekock@winter>
 *  DESCR:  	runs function <fn> on every item in table
 */
void HASH_Process(HASH_TABLE *h, HASH_ProcessProc fn)
{
   unsigned index;

#ifdef HASH_DEBUG
   if (!h || !fn) {
      if (HASH_Handler) { (HASH_Handler)(h, HASH_BadArgument, "HASH_Process()"); }
      return;
   }
   if (h->attr & HASH_ReportAccess) {
      printf("HASH TABLE: 0x%.8x Process(0x%.8x) started\n", h, fn);
   }
#endif
   
   if (h->attr & HASH_Intrusive) {
      DATA_PTR data;
      for (index = 0; index < h->size; index++) {
	 for (data = h->array.data[index]; data; data = NextP(h, data)) {
	    (fn)(data);
	 }
      }
   }
   else {
      HASH_CONTAINER *cont;
      for (index = 0; index < h->size; index++) {
	 for (cont = h->array.cont[index]; cont; cont = cont->next) {
	    (fn)(cont->data);
	 }
      }
   }
#ifdef HASH_DEBUG
   if (h->attr & HASH_ReportAccess) {
      printf("HASH TABLE: 0x%.8x Process(0x%.8x) complete\n", h, fn);
   }
#endif

#ifdef _HASH_INTERNAL_DEBUG
   HASH_Verify(h);
#endif

}


/*-----------------------------------------------------------
 *  Name: 	HASH_ProcessPlus()
 *  Created:	Thu Sep  8 20:42:48 1994
 *  Author: 	Jonathan DeKock   <dekock@winter>
 *  DESCR:  	process a hash table with second argument
 */
void HASH_ProcessPlus(HASH_TABLE *h, HASH_ProcessPlusProc fn, DATA_PTR arg2)
{
   unsigned index;

#ifdef HASH_DEBUG
   if (!h || !fn) {
      if (HASH_Handler) { (HASH_Handler)(h, HASH_BadArgument, "HASH_ProcessPlus()"); }
      return;
   }
   if (h->attr & HASH_ReportAccess) {
      printf("HASH TABLE: 0x%.8x ProcessPlus(0x%.8x, 0x%.8x) started\n", h, fn, arg2);
   }
#endif
   
   if (h->attr & HASH_Intrusive) {
      DATA_PTR data;
      for (index = 0; index < h->size; index++) {
	 for (data = h->array.data[index]; data; data = NextP(h, data)) {
	    (fn)(data, arg2);
	 }
      }
   }
   else {
      HASH_CONTAINER *cont;
      for (index = 0; index < h->size; index++) {
	 for (cont = h->array.cont[index]; cont; cont = cont->next) {
	    (fn)(cont->data, arg2);
	 }
      }
   }
   
#ifdef HASH_DEBUG
   if (h->attr & HASH_ReportAccess) {
      printf("HASH TABLE: 0x%.8x ProcessPlus(0x%.8x, 0x%.8x) complete\n", h, fn, arg2);
   }
#endif

#ifdef _HASH_INTERNAL_DEBUG
   HASH_Verify(h);
#endif

}


/*-----------------------------------------------------------
 *  Name: 	HASH_GetNext()
 *  Created:	Thu Sep  8 20:46:38 1994
 *  Author: 	Jonathan DeKock   <dekock@winter>
 *  DESCR:  	iteration support for hash tables
 *  Modified to have last_cont in HASH_TABLE, instead of static.
 *  still we CAN NOT do HASH_Iterate on the same hash_table, though.
 */
DATA_PTR HASH_GetNext(HASH_TABLE *h, DATA_PTR current)
{
   unsigned        index;
   DATA_PTR        data;
   HASH_CONTAINER *cont = NULL;

#ifdef HASH_DEBUG
   if (!h) {
      if (HASH_Handler) { (HASH_Handler)(h, HASH_BadArgument, "HASH_Iterate()"); }
      return(NULL);
   }
#endif

   if (!current) { /* Find first item */

      for (index = 0; ((index < h->size) && (!(h->array.data[index]))); index++);
      if (index < h->size) {
	 if (h->attr & HASH_Intrusive) {
	    data = h->array.data[index];
	 }
	 else {
	    cont = h->array.cont[index];
	    data = cont->data;
	 }
      }
      else {
	 data = NULL; /* table is empty */
      }
   }
   else {
      if (h->attr & HASH_Intrusive) {
	 data = NextP(h, current);
	 if (!data) { /* current was last one on last_index */
	    index = (h->hash)(KeyP(h, current), h->size);
#ifdef HASH_DEBUG
	    if (index >= h->size) {
	       if (HASH_Handler) { (HASH_Handler)(h, HASH_BadIndex, "HASH_Iterate()"); }
	       return(NULL);
	    }
#endif
	    for (index++; ((index < h->size) && (!(h->array.data[index]))); index++);
	    if (index < h->size) data = h->array.data[index];
	    else data = NULL; /* current was last one in table */
	 }
      }
      else {
#if	0
	 HASH_CONTAINER *cont;
#endif
	 if ((h->last_cont) && (h->last_cont->data == current)) cont = h->last_cont;
	 else { /* LAST_CONTAINER is messed up ?? */
	    index = (h->hash)(KeyP(h, current), h->size);  /* get index of current and find its cont */
#ifdef HASH_DEBUG
	    if (index >= h->size) {
	       if (HASH_Handler) { (HASH_Handler)(h, HASH_BadIndex, "HASH_Iterate()"); }
	       return(NULL);
	    }
#endif
	    for (cont = h->array.cont[index]; 
	    	 ((cont) && (cont->data != current)); 
		 cont = cont->next);
	 }
#ifdef HASH_DEBUG
	 if (!cont) { /* failed to find current's container must have been deleted!!!! */
	    if (HASH_Handler) { (HASH_Handler)(h, HASH_BadArgument, "HASH_Iterate()"); }
	    return(NULL);
	 }
#endif
	 if (!cont->next) { /* current was last one in index */
	    index = (h->hash)(KeyP(h, cont->data), h->size);
	    for (index++; ((index < h->size) && (!(h->array.cont[index]))); index++);
	    if (index < h->size) cont = h->array.cont[index];
	    else cont = NULL; /* current is the absolute LAST */
	 }
	 else {
	    cont = cont->next;
	 }
	 if (cont) data = cont->data;
	 else data = NULL;
      }
   }
      h->last_cont = cont;
#ifdef HASH_DEBUG
   if (h->attr & HASH_ReportAccess) {
      printf("HASH TABLE: 0x%.8x Iterate() currently at 0x%.8x\n", h, data);
   }
#endif

#ifdef _HASH_INTERNAL_DEBUG
   HASH_Verify(h);
#endif

   return(data);
}
	    
/*-----------------------------------------------------------
 *  Name: 	HASH_SetHandler()
 *  Created:	Fri Sep  9 00:12:23 1994
 *  Author: 	Jonathan DeKock   <dekock@winter>
 *  DESCR:  	sets the hash table handler
 */
HASH_ErrorProc HASH_SetHandler(HASH_ErrorProc fn, char *name)
{
   HASH_ErrorProc tmp;
#ifdef HASH_DEBUG
   if (name) printf("HASH TABLE: Handler changed to %s() = 0x%.8x\n", name, fn);
   if (name) printf("HASH TABLE: Handler changed to = 0x%.8x\n", fn);
#endif
   tmp = HASH_Handler;
   HASH_Handler = fn;
   return(tmp);
}


/*-----------------------------------------------------------
 *  Name: 	HASH_DefaultHandler()
 *  Created:	Fri Sep  9 00:13:56 1994
 *  Author: 	Jonathan DeKock   <dekock@winter>
 *  DESCR:  	default Handler for a hash table
 */
void HASH_DefaultHandler(HASH_TABLE *h, enum HASH_ERROR num, char *fn)
{
#ifdef HASH_DEBUG
   FILE *out = stdout;
#else
   FILE *out = stderr;
#endif
   fflush(stdout);
   fflush(stderr);
   fprintf(out, "HASH TABLE: 0x%.8x Error in function %s: \"%s\"\n", (u_int)h, fn, HASH_errlist[num]);
   fflush(out);
}

/*-----------------------------------------------------------
 *  Name: 	HASH_Verify()
 *  Created:	Wed Sep 14 13:58:27 1994
 *  Author: 	Jonathan DeKock   <dekock@winter>
 *  DESCR:  	verifies a hash table
 */
int HASH_Verify(HASH_TABLE *h)
{
   int ret = 1;
   unsigned index;
   unsigned count = 0;
   
   if (h->attr & HASH_Intrusive) {
      DATA_PTR data;
      for (index = 0; index < h->size; index++) {
	 for (data = h->array.data[index]; data; data = NextP(h, data)) {
	    count++;
	    /* check that key hashes to index */
	    if (((h->hash)(KeyP(h, data), h->size)) != index) {
	       ret = 0;
#ifdef _HASH_INTERNAL_DEBUG
	       printf("HASH TABLE: 0x%.8x Verify() found 0x%.8x (key 0x%.8x = %u) incorrectly hashed at %u\n",
		      h, data, KeyP(h, data), (h->hash)(KeyP(h, data),h->size), index);
#endif
	       if (HASH_Handler) {  (HASH_Handler)(h, HASH_TableCorrupted, "HASH_Verify()");       }
	    }
	 }
      }
   }
   else {
      HASH_CONTAINER *cont;
      for (index = 0; index < h->size; index++) {
	 for (cont = h->array.cont[index]; cont; cont = cont->next) {
	    count++;
	    if (!cont->data) {
	       ret = 0;
#ifdef _HASH_INTERNAL_DEBUG
	       printf("HASH TABLE: 0x%.8x Verify() found container 0x%.8x without data\n", h, cont);
#endif
	       if (HASH_Handler) {  (HASH_Handler)(h, HASH_TableCorrupted, "HASH_Verify()");       }
	    }
	    /* check that key hashes to index */
	    if (((h->hash)(KeyP(h, cont->data), h->size)) != index) {
	       ret = 0;
#ifdef _HASH_INTERNAL_DEBUG
	       printf("HASH TABLE: 0x%.8x Verify() found 0x%.8x (key 0x%.8x = %u) incorrectly hashed at %u\n",
		      h, cont->data, KeyP(h, cont->data), (h->hash)(KeyP(h, cont->data), h->size), index);
#endif
	       if (HASH_Handler) {  (HASH_Handler)(h, HASH_TableCorrupted, "HASH_Verify()");       }
	    }
	 }
      }
   }

   if (count != h->count) {
      ret = 0;
#ifdef _HASH_INTERNAL_DEBUG
      printf("HASH TABLE: 0x%.8x Verify() found count incorrect at %u, table thinks %u\n", h, count, h->count);
#endif
      if (HASH_Handler) { (HASH_Handler)(h, HASH_TableCorrupted, "HASH_Verify()");      }
   }
   
#ifdef _HASH_INTERNAL_DEBUG
   if (!ret) printf("HASH TABLE: 0x%.8x is NOT valid\n", h);
#endif
   
   return(ret);
}

/*-----------------------------------------------------------
 *  Name: 	HASH_Print()
 *  Created:	Fri Sep  9 00:18:24 1994
 *  Author: 	Jonathan DeKock   <dekock@winter>
 *  DESCR:  	prints a table out.
 */
void HASH_Print(HASH_TABLE *h, HASH_PrintProc print)
{
   unsigned index;
   if (!print) print = (HASH_PrintProc) HASH_DefaultPrinter;

   HASH_Verify(h);
   
   printf("HASH TABLE: 0x%.8x attributes: %s, %s, %s\n", (u_int)h,
	  (h->attr & HASH_Intrusive) ? "Intrusive" : "Container",
	  (h->attr & HASH_ReportChange) ? "Report Change" : "No Change Report",
	  (h->attr & HASH_ReportAccess) ? "Report Access" : "No Access Report");
   printf("HASH TABLE: 0x%.8x size = %u\n", (u_int)h, h->size);
   if (h->attr & HASH_Intrusive) {
      printf("HASH TABLE: 0x%.8x key offset = %u, next offset = %u\n",
	     (u_int)h, h->key_offset, h->next_offset);
   }
   else {
      printf("HASH TABLE: 0x%.8x key offset = %u\n", (u_int)h, h->key_offset);
   }
   printf("HASH TABLE: 0x%.8x functions: hash = 0x%.8x, lookup = 0x%.8x, destroy = 0x%.8x\n",
	  (u_int)h, (u_int)h->hash, (u_int)h->lookup, (u_int)h->destroy);
   

   if (h->attr & HASH_Intrusive) {
      DATA_PTR data;
      for (index = 0; index < h->size; index++) {
	 for (data = h->array.data[index]; data; data = NextP(h, data)) {
	    (print)(index, data, KeyP(h, data)); 
	 }
      }
   }
   else {
      HASH_CONTAINER *cont;
      for (index = 0; index < h->size; index++) {
	 for (cont = h->array.cont[index]; cont; cont = cont->next) {
	    (print)(index, cont->data, KeyP(h, cont->data));
	 }
      }
   }
}


/*-----------------------------------------------------------
 *  Name: 	HASH_DefaultPrinter()
 *  Created:	Wed Sep 14 13:56:13 1994
 *  Author: 	Jonathan DeKock   <dekock@winter>
 *  DESCR:  	default printer for hash table elt
 */
void HASH_DefaultPrinter(unsigned index, DATA_PTR data, DATA_PTR key)
{
   printf("%3d: 0x%.8x key = 0x%.8x = \"%s\"\n", index, (u_int)data, (u_int)key, (char *)key);
}   


/*-----------------------------------------------------------
 *  Name: 	HASH_GetContainer()
 *  Created:	Thu Sep 15 20:37:36 1994
 *  Author: 	Jonathan DeKock   <dekock@winter>
 *  DESCR:  	gets a hash's container
 */
HASH_CONTAINER *HASH_GetContainer(HASH_TABLE *h, DATA_PTR data)
{
   HASH_CONTAINER *cont = NULL;
   unsigned index;

#ifdef HASH_DEBUG
   if (!h || !data) {
      if (HASH_Handler) { (HASH_Handler)(h, HASH_BadArgument, "HASH_GetContainer()"); }
      return(NULL);
   }
   if (h->attr & HASH_Intrusive) {
      if (HASH_Handler) { (HASH_Handler)(h, HASH_TableCorrupted, "HASH_GetContainer()"); }
      return(NULL);
   }
#endif
   
   for (index = 0; index < h->size; index++) {
      for (cont = h->array.cont[index]; cont; cont = cont->next) {
	 if (cont->data == data) break;
      }
      if (cont) break;
   }
   return(cont);
}

/*-----------------------------------------------------------
 *  Name: 	HASH_DefaultHashFunction()
 *  Created:	Fri Sep  9 00:50:26 1994
 *  Author: 	Jonathan DeKock   <dekock@winter>
 *  DESCR:  	attempts to randomly hash a character string
 */
unsigned HASH_DefaultHashFunction(char *key, unsigned size)
{
   register unsigned long int ret = 0;
   do {
      ret = ((ret << 1) ^ (((unsigned)*key)*31));
   } while (*(++key));
   
   return(ret%size);
}

/*-----------------------------------------------------------
 *  Name: 	HASH_DefaultLookupFunction()
 *  Created:	Fri Sep  9 01:06:56 1994
 *  Author: 	Jonathan DeKock   <dekock@winter>
 *  DESCR:  	lookup function for char strings
 */
int HASH_DefaultLookupFunction(char *k1, char *k2)
{
   for (; ((*k1) && ((*k1) == (*k2))); k1++, k2++);
   return(!(*k1 | *k2));
}

