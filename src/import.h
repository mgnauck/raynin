#ifndef IMPORT_H
#define IMPORT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct mesh mesh;
typedef struct scene scene;

uint8_t import_gltf(scene *s, const char *gltf, size_t gltf_sz, const uint8_t *bin, size_t bin_sz);

void    import_mesh(mesh *m, const uint8_t *data, uint32_t mtl, bool faceNormals);

#endif
