/*-----------------------------------------------------------
 *  Name: 	array.c
 *  Created:	Tue Sep  6 18:45:10 1994
 *  Author: 	Jonathan DeKock   <dekock@winter>
 *  DESCR:  	funcitons for operation on arrays of pointers
 */

#include <New.h>
#include <stack.h>
#include <array.h>
#include <stddef.h>

/*-----------------------------------------------------------
 *  Name: 	ARRAY_BinarySearch()
 *  Created:	Tue Sep  6 18:48:16 1994
 *  Author: 	Jonathan DeKock   <dekock@winter>
 *  DESCR:  	performs binary search on an array
 */
DATA_PTR ARRAY_BinarySearch(DATA_PTR in, unsigned size, DATA_PTR key, DATA_PTR fn)
{
   DATA_PTR* array = (DATA_PTR*)in;
   ARRAY_SearchProc compare = (ARRAY_SearchProc)fn;
   unsigned min, max, index;
   int result;
#ifdef ARRAY_DEBUG
   unsigned count = 0;
   if (!array || !compare) {
      printf("ARRAY: 0x%.8x (%u) BinarySearch(0x%.8x, 0x%.8x) failed: \"invalid argument\"\n",
	     array, size, key, compare);
      fflush(stdout);
      return(NULL);
   }
#endif
   
   min = 0;
   max = size - 1;

#ifdef ARRAY_DEBUG
   printf("ARRAY: 0x%.8x (%u) BinarySearch(0x%.8x, 0x%.8x)\n", array, size, key, compare);
   printf("ARRAY: 0x%.8x Index: ", array);
#endif

   for (index = (min+max)/2; min <= max; index = (min+max)/2) {
      result = (compare)(array[index], key);
#ifdef ARRAY_DEBUG
      count++;
      printf("%u ", index);
      if (!result) printf("=> 0x%.8x (%u comparisons)\n", array[index], count);
      fflush(stdout);
#endif
      /* if the array[index] == key return data */
      if (!result) { return(array[index]); }

      /* if the array[index] < key then look higher in array */
      else if (result < 0) { min = index + 1; }

      /* if the array[index] > key then look lower in array */
      else { max = index - 1; }
   }
   
#ifdef ARRAY_DEBUG
   printf("not found (%u comparisons)\n", count);
   fflush(stdout);
#endif
   
   return(NULL); /* unable to find */
}

/*-----------------------------------------------------------
 *  Name: 	ARRAY_Find()
 *  Created:	Tue Sep  6 18:58:34 1994
 *  Author: 	Jonathan DeKock   <dekock@winter>
 *  DESCR:  	search thru the entire array looking for a match to <key>
 */
DATA_PTR ARRAY_Find(DATA_PTR in, unsigned size, DATA_PTR key, DATA_PTR fn)
{
   DATA_PTR* array = (DATA_PTR*)in;
   ARRAY_FindProc compare = (ARRAY_FindProc)fn;
   unsigned index;
   int result;

#ifdef ARRAY_DEBUG
   if (!array || !compare) {
      printf("ARRAY: 0x%.8x (%u) Find(0x%.8x, 0x%.8x) failed: \"invalid argument\"\n",
	     array, size, key, compare);
      fflush(stdout);
      return(NULL);
   }
#endif
   
   for (index = 0; index < size; index++) {
      result = (compare)(array[index], key);
#ifdef ARRAY_DEBUG
      if (result) printf("ARRAY: 0x%.8x (%u) Find(0x%.8x, 0x%.8x) => 0x%.8x (%u)\n",
			 array, size, key, compare, array[index], index);
#endif
      if (result) { return(array[index]); } /* Found it */
   }
#ifdef ARRAY_DEBUG
      if (result) printf("ARRAY: 0x%.8x (%u) Find(0x%.8x, 0x%.8x) not found\n",
			 array, size, key, compare);
#endif
   return(NULL); /* unable to find */
}

/*-----------------------------------------------------------
 *  Name: 	ARRAY_Sort()
 *  Created:	Tue Sep  6 19:02:08 1994
 *  Author: 	Jonathan DeKock   <dekock@winter>
 *  DESCR:  	picks the best sort array for your data
 */
void ARRAY_Sort(DATA_PTR array, unsigned size, DATA_PTR compare)
{
   if (size < 50) { ARRAY_BubbleSort(array, size, compare); }
   else if (size < 100) { ARRAY_QuickSort(array, size, compare); }
   else { ARRAY_MergeSort(array, size, compare); }
}


/*-----------------------------------------------------------
 *  Name: 	ARRAY_BubbleSort()
 *  Created:	Tue Sep  6 19:04:34 1994
 *  Author: 	Jonathan DeKock   <dekock@winter>
 *  DESCR:  	bubble sorts an array
 */
void ARRAY_BubbleSort(DATA_PTR in, unsigned size, DATA_PTR fn)
{
   DATA_PTR* array = (DATA_PTR*)in;
   ARRAY_CompareProc compare = (ARRAY_CompareProc)fn;
   unsigned index;
   int      bubble;
   int result;
   DATA_PTR tmp;
#ifdef ARRAY_DEBUG
   unsigned count = 0;
   if (!array || !compare) {
      printf("ARRAY: 0x%.8x (%u) BubbleSort(0x%.8x) failed: \"invalid argument\"\n",
	     array, size, compare);
      fflush(stdout);
      return;
   }
#endif
   
   for (index = 1; index < size; index++) {
      tmp = array[index];
      for (bubble = index - 1; bubble >= 0; bubble--) {
	 result = (compare)(array[bubble], tmp);
#ifdef ARRAY_DEBUG
	 count++;
#endif
	 if (result > 0) { /* tmp belongs before bubble */
	    array[bubble+1] = array[bubble]; /* perform the exchange */
	    array[bubble] = tmp;
	 }
	 else {
	    break; /* we have bubbled far enough */
	 }
      }
   }
#ifdef ARRAY_DEBUG
   printf("ARRAY: 0x%.8x (%u) BubbleSort(0x%.8x) complete (%u comparisons)\n",
	  array, size, compare, count);
   fflush(stdout);
#endif
}

/*-----------------------------------------------------------
 *  Name: 	ARRAY_QuickSort()
 *  Created:	Tue Sep  6 19:19:22 1994
 *  Author: 	Jonathan DeKock   <dekock@winter>
 *  DESCR:  	quick-sorts an array
 */
void ARRAY_QuickSort(DATA_PTR in, unsigned size, DATA_PTR fn)
{
   DATA_PTR* array = (DATA_PTR*)in;
   ARRAY_CompareProc compare = (ARRAY_CompareProc)fn;
   unsigned p, r;
   STACK *stack;
#ifdef ARRAY_DEBUG
   unsigned count = 0;
   if (!array || !compare) {
      printf("ARRAY: 0x%.8x (%u) QuickSort(0x%.8x) failed: \"invalid argument\"\n",
	     array, size, compare);
      fflush(stdout);
      return;
   }
#endif
   if (size <= 1) return;

   stack = Create_STACK(2*size);  /* static stack of worst case size */
   
   if (!stack) { /* failed to allocate memory */
      ARRAY_BubbleSort(array, size, (DATA_PTR) compare); /* Force Bubble sort, which needs no memory */
      return;
   }

   r = size-1;
   p = 0;

   while (p < size - 1) {
      if (p < r) {
	 DATA_PTR ctrl = array[(p+r)/2]; /* Pick a point in the middle of array p...r */
	 DATA_PTR tmp;
	 int left  = p - 1;
	 int right = r + 1;
	 while (left < right) {
	    do {
	       right--;
#ifdef ARRAY_DEBUG
	       if (array[right] != ctrl) count++;
#endif
	    } while((array[right] != ctrl) && (((compare)(array[right], ctrl)) > 0)); 
	    do {
	       left++;
#ifdef ARRAY_DEBUG
	       if (array[left] != ctrl) count++;
#endif
	    } while((array[left] != ctrl) && (((compare)(array[left],  ctrl)) < 0)); 
	    if (left < right) { /* exchange the two */
	       tmp          = array[left];
	       array[left]  = array[right];
	       array[right] = tmp;
	    }
	 }
	 PushM(stack, right+1); /* Push for future loop on array a[right+1] ... a[r] */
	 PushM(stack, r);
	 r = right;             /* run the loop on array a[p] ... a[right] */
      }
      else {
	 r = (unsigned) PopM(stack); /* run previously pushed loop */
	 p = (unsigned) PopM(stack);
      }
   }
   Destroy_STACK(stack);
#ifdef ARRAY_DEBUG
   printf("ARRAY: 0x%.8x (%u) QuickSort(0x%.8x) complete (%u comparisons)\n",
	  array, size, compare, count);
   fflush(stdout);
#endif
}

/*-----------------------------------------------------------
 *  Name: 	ARRAY_MergeSort()
 *  Created:	Wed Sep  7 01:26:06 1994
 *  Author: 	Jonathan DeKock   <dekock@winter>
 *  DESCR:  	merge sorts an array
 */
void ARRAY_MergeSort(DATA_PTR in, unsigned size, DATA_PTR fn)
{
   DATA_PTR* array = (DATA_PTR*)in;
   ARRAY_CompareProc compare = (ARRAY_CompareProc)fn;
   STACK    *stack;
   DATA_PTR *tmp;
   unsigned p, q, r, i1, i2, it;
#ifdef ARRAY_DEBUG
   unsigned count = 0;
   if (!array || !compare) {
      printf("ARRAY: 0x%.8x (%u) MergeSort(0x%.8x) failed: \"invalid argument\"\n",
	     array, size, compare);
      fflush(stdout);
      return;
   }
#endif

   if (size <= 1) return;  /* allready sorted */

   p = size;
   q = 1;
   while (p) { q++; p = p >> 1; } /* approx on log2(size) */
   stack = Create_STACK(4*q);
   if (!stack) { ARRAY_BubbleSort(array, size, (DATA_PTR) compare); return; }
   
   tmp = NewArray(DATA_PTR, size);
   if (!tmp)   { Destroy_STACK(stack); ARRAY_BubbleSort(array, size, (DATA_PTR) compare); return; }

   p = 0;
   r = size - 1;
   PushM(stack, 0);  /* keep from going under stack on last loop */
   PushM(stack, 0);
   while ((unsigned)tmp[0] != size-1) {
      if ((unsigned)tmp[p] == r) { /* AUTO-MERGE, all sub-arrays processed */
	 q = r;
	 r = (unsigned)tmp[q+1];
	 for (i1 = p, i2 = q+1, it = p; it <= r; it++) { /* MERGE */
	    if (i1 > q) {
	       tmp[it] = array[i2++];
	    }
	    else if (i2 > r) {
	       tmp[it] = array[i1++];
	    }
	    else if (((compare)(array[i1], array[i2])) < 0) {
	       tmp[it] = array[i1++];
#ifdef ARRAY_DEBUG
	       count++;
#endif
	    }
	    else {
	       tmp[it] = array[i2++];
#ifdef ARRAY_DEBUG
	       count++;
#endif
	    }
	 }
	 for (it = p; it <= r; it++) { array[it] = tmp[it]; } /* place back in array */
	 tmp[p] = (DATA_PTR)r;
	 r = (unsigned) PopM(stack);
	 p = (unsigned) PopM(stack);
      }
      else if (r-p > 1) { /* Push for later eval. */
	 q = (r+p)/2;
	 PushM(stack, p);
	 PushM(stack, q);
	 PushM(stack, q+1);
	 PushM(stack, r);
	 r = q;
      }
      else if (r - p == 1) { /* Two items in sub-array, merge them */
#ifdef ARRAY_DEBUG
	 count++;
#endif
	 if (((compare)(array[p], array[r])) > 0) {
	    DATA_PTR d = array[p];
	    array[p] = array[r];
	    array[r] = d;
	 }
	 tmp[p] = (DATA_PTR)r;
	 r = (unsigned) PopM(stack);
	 p = (unsigned) PopM(stack);
      }
      else { /* One item on array, declare it sorted */
	 tmp[p] = (DATA_PTR)p;
	 r = (unsigned) PopM(stack);
	 p = (unsigned) PopM(stack);
      }
   }
   Delete(tmp);
   Destroy_STACK(stack);
#ifdef ARRAY_DEBUG
   printf("ARRAY: 0x%.8x (%u) MergeSort(0x%.8x) complete (%u comparisons)\n",
	  array, size, compare, count);
   fflush(stdout);
#endif
}
