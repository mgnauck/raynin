#ifndef SUTIL_H
#define SUTIL_H

#include <stddef.h>
#include "settings.h"

#ifndef NATIVE_BUILD

void *malloc(size_t size);
void free(void *ptr);

void *memset(void *dest, int c, size_t cnt);
void *memcpy(void *dest, const void *src, size_t cnt);

#else

#include <stdlib.h>
#include <string.h>

#endif

#endif
