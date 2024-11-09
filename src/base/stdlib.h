#ifndef STDLIB_H
#define STDLIB_H

#include <stddef.h>

#ifdef BUMP_ALLOCATOR

__attribute__((visibility("default")))
void *malloc(size_t size);

void free(void *ptr);

#else

#include "../ext/walloc.h"

#endif

#endif 
