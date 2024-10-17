#ifndef STRING_H
#define STRING_H

#include <stddef.h>

void 		*memset(void *dest, int c, size_t count);
void 		*memcpy(void *dest, const void *src, size_t count);
char 		*strcat(char *destination, const char *source);

size_t          strlen(const char *p);
int             strcmp(const char *s1, const char *s2);
int             strncmp(const char *s1, const char *s2, size_t n);
char            *strstr(const char *str, const char *sub);

int             tolower(int c);

extern float    atof(const char *s); // Imported from JS
int             atoi(const char *s);

char            *strstr_lower(const char *str, const char *sub);

#endif
