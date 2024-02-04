#ifndef MESH_H
#define MESH_H

#include <stddef.h>

typedef struct vec3 vec3;
typedef struct tri tri;
typedef struct tri_data tri_data;

typedef struct mesh {
  size_t    tri_cnt;
  tri       *tris;
  tri_data  *tris_data;
} mesh;

void mesh_init(mesh *m, size_t tri_cnt);

#endif
