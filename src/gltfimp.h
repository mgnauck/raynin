#ifndef GLTFIMP_H
#define GLTFIMP_H

#include <stddef.h>

typedef struct scene scene;

int gltf_import(scene *s, const char *gltf, size_t gltf_sz, const unsigned char *bin, size_t bin_sz);

#endif
