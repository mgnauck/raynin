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
} mesh;

void mesh_init(mesh *m, uint32_t tri_cnt);

void mesh_read(mesh *m, const uint8_t *data);

void mesh_make_quad(mesh *m, vec3 pos, vec3 nrm, float w, float h);
void mesh_make_icosphere(mesh *m, uint8_t steps, bool face_normals);

void mesh_release(mesh *m);

#endif
