#include "sutil.h"

#ifndef NATIVE_BUILD

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

size_t strlen(const char *p)
{
  size_t len = 0;
  while(*p++) { len++; }
  return len;
}

int strcmp(const char *s1, const char *s2)
{
  while(*s1 == *s2++)
    if(*s1++ == 0)
      return 0;
  return (*(const unsigned char *)s1 - *(const unsigned char *)(s2 - 1));
}

int strncmp(const char *s1, const char *s2, size_t n)
{
  for(size_t i=0; i<n; i++) {
    int d = (unsigned char)s1[i] - (unsigned char)s2[i];
    if(d != 0 || s1[i] == '\0')
      return d;
  }
  return 0;
}

float atof(const char* s)
{
  float res = 0.0f;
  float fact = 1;
  
  if(*s == '-') {
    s++;
    fact = -1;
  }

  for(int pt_seen=0; *s; s++) {
    if(*s == '.') {
      pt_seen = 1;
      continue;
    }
    int d = *s - '0';
    if(d >= 0 && d <= 9) {
      if(pt_seen)
        fact /= 10.0f;
      res = res * 10.0f + (float)d;
    }
  }
  return res * fact;
}

#endif
