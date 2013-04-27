#include <string.h>
#include <irrdmem.h>

DATA_PTR irrd_malloc(unsigned size)
{
  DATA_PTR data = (char *) malloc(size);                       
  if (!data)
      return NULL;

  memset((char *)data, '\0', size);
  return data;
}

void irrd_free(DATA_PTR ptr)
{
  free(ptr);
}
