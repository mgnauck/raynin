#ifndef GLTFIMP_H
#define GLTFIMP_H

#include <stddef.h>
#include <stdint.h>

typedef struct scene scene;

uint8_t gltf_import(scene *s, const char *gltf, size_t gltf_sz, const uint8_t *bin, size_t bin_sz);

#endif
