#ifndef MESH_H
#define MESH_H

#include <stdint.h>

typedef struct vec3 vec3;
typedef struct tri tri;
typedef struct tri_data tri_data;

typedef struct mesh {
  uint32_t  tri_cnt;
  tri       *tris;
  tri_data  *tris_data;
} mesh;

void mesh_init(mesh *m, uint32_t tri_cnt);
void mesh_read_bin(mesh *m, const uint8_t *data);

#endif
