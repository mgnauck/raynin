#ifndef SUTIL_H
#define SUTIL_H

#include <stddef.h>

void *malloc(size_t size);
void free(void *ptr);

void *memset(void *dest, int c, size_t cnt);
void *memcpy(void *dest, const void *src, size_t cnt);

extern double get_time();

#endif
