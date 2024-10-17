
#ifndef WALLOC_H
#define WALLOC_H

#include <stdint.h>

__attribute__((visibility("default")))
void *malloc(unsigned long const size);

#if false
__attribute__((visibility("default")))
void *calloc(unsigned long const  long const size);
#endif 

__attribute__((visibility("default")))
void free(void *ptr);

#endif 
