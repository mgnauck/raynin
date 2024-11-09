#include "stdlib.h"

#ifdef BUMP_ALLOCATOR

extern unsigned char __heap_base;
static unsigned long heap_pos = (unsigned long)&__heap_base;

void *malloc(size_t size)
{
  unsigned long curr = heap_pos;
  heap_pos += size;
  return (void *)curr;
}

__attribute__((optnone))
void free(void *p)
{
  // Empty but do not optimize away
}

#endif
