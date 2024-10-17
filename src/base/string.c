#include "string.h"

// requires -mbulk-memory + --enable-bulk-memory
void *memset(void *dest, int c, size_t count)
{
    return __builtin_memset(dest, c, count);
}

void *memcpy(void *dest, const void *src, size_t count)
{
    return __builtin_memcpy(dest, src, count);
}

char *strcat(char *destination, const char *source)
{
    return memcpy(destination + strlen(destination), source, strlen(source));
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

char *strstr(const char *str, const char *sub)
{
  int l = strlen(sub);
  char *p = (char *)sub;
  while(*str && *p) {
    if(*str++ == *p)
      p++;
    if(!*p)
      return (char *)str - l;
    if(l == p - sub)
      p = (char *)sub;
  }
  return NULL;
}

int tolower(int c)
{
  return c > 0x40 && c < 0x5b ? c | 0x60 : c;
}

int atoi(const char *s)
{
  int i = 0;
  while(*s) {
    i = i * 10 + *s - '0';
    s++;
  }
  return i;
}

char *strstr_lower(const char *str, const char *sub)
{
  int l = strlen(sub);
  char *p = (char *)sub;
  while(*str && *p) {
    if(tolower(*str) == tolower(*p))
      p++;
    str++;
    if(!*p)
      return (char *)str - l;
    if(l == p - sub)
      p = (char *)sub;
  }
  return NULL;
}
