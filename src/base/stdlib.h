#ifndef STDLIB_H
#define STDLIB_H

#include <stddef.h>

__attribute__((visibility("default")))
void *malloc(size_t size);

void free(void *ptr);

#endif 
