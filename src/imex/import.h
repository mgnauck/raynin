#ifndef IMPORT_H
#define IMPORT_H

#include <stddef.h>
#include <stdint.h>

typedef struct scene scene;

uint8_t import_gltf(scene *s, const char *gltf, size_t gltf_sz,
                    const uint8_t *bin, size_t bin_sz);

#endif
