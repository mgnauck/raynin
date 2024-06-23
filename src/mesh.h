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
  bool      is_emissive;
  uint32_t  ofs;
} mesh;

void mesh_init(mesh *m, uint32_t tri_cnt);
void mesh_release(mesh *m);

void mesh_create_quad(mesh *m, uint32_t subx, uint32_t suby, uint32_t mtl);
void mesh_create_box(mesh *m, uint32_t mtl);
void mesh_create_icosphere(mesh *m, uint8_t steps, uint32_t mtl, bool face_normals);
void mesh_create_uvsphere(mesh *m, float radius, uint32_t subx, uint32_t suby, uint32_t mtl, bool face_normals);
void mesh_create_uvcylinder(mesh *m, float radius, float height, uint32_t subx, uint32_t suby, uint32_t mtl, bool face_normals);
void mesh_create_torus(mesh *m, float inner_radius, float outer_radius, uint32_t sub_inner, uint32_t sub_outer, uint32_t mtl, bool face_normals);

#endif
