#ifndef WALLOC_H
#define WALLOC_H

#ifndef BUMP_ALLOCATOR

#include <stddef.h>

__attribute__((visibility("default")))
void *malloc(size_t size);

__attribute__((visibility("default")))
void *calloc(size_t num, size_t size);

__attribute__((visibility("default")))
void free(void *ptr);

#endif

#endif 
