#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include "fdutils.h"


void *safe_calloc(size_t nmemb, size_t size)
{
  void *ptr;

  ptr = calloc(nmemb, size);
  if(!ptr) {
    fprintf(stderr,"Out of memory error\n");
    exit(1);
  }
  return ptr;
}


void *safe_malloc(size_t size)
{
  void *ptr;

  ptr = malloc(size);

  if(!ptr) {
    fprintf(stderr,"Out of memory error\n");
    exit(1);
  }
  return ptr;
}
