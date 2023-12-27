#include "util.h"

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

void *memset(void *dest, int c, size_t cnt)
{
  return __builtin_memset(dest, c, cnt);
}

void *memcpy(void *dest, const void *src, size_t cnt)
{
  return __builtin_memcpy(dest, src, cnt);
}
