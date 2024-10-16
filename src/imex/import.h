#ifndef IMPORT_H
#define IMPORT_H

#include <stddef.h>
#include <stdint.h>

typedef struct scene scene;
typedef struct escene escene;

uint8_t import_gltf(scene *s, const char *gltf, size_t gltf_sz,
                    const uint8_t *bin, size_t bin_sz);
uint8_t import_gltf_ex(escene *s, const char *gltf, size_t gltf_sz,
                       const uint8_t *bin, size_t bin_sz);

uint8_t import_bin(scene **scenes, uint8_t *scene_cnt, const uint8_t *bin);

#endif
