#ifndef MESH_H
#define MESH_H

#include <stdbool.h>
#include <stdint.h>

typedef struct vec3 vec3;
typedef struct tri tri;

typedef struct mesh {
  uint32_t  tri_cnt;
  tri       *tris;
  vec3      *centers;
  uint32_t  ofs;
} mesh;

void mesh_init(mesh *m, uint32_t tri_cnt);
void mesh_release(mesh *m);

#endif
