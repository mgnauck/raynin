#ifndef STRING_H
#define STRING_H

#include <stddef.h>

void          *memset(void *dest, int c, size_t cnt);
void          *memcpy(void *dest, const void *src, size_t cnt);

char          *strcat(char *dest, const char *src);

size_t        strlen(const char *p);
int           strcmp(const char *s1, const char *s2);
int           strncmp(const char *s1, const char *s2, size_t n);
char          *strstr(const char *str, const char *sub);
char          *strstr_lower(const char *str, const char *sub);
int           tolower(int c);

int           atoi(const char *s);
extern float  atof(const char *s);

#endif
