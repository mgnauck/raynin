#ifndef MESH_H
#define MESH_H

#include <stddef.h>
#include "mat.h"

typedef struct vec3 vec3;
typedef struct tri tri;
typedef struct bvh bvh;

typedef struct mesh {
  size_t    tri_cnt;
  tri       *tris;
  vec3      *centers;
  bvh       *bvh;
  mat_type  mat_type;
  size_t    mat_ofs;
} mesh;

mesh  *mesh_create(vec3 *data, size_t data_size);
void  mesh_release(mesh *m);

#endif
