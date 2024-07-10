#ifndef MESH_H
#define MESH_H

#include <stdbool.h>
#include <stdint.h>

#define QUAD_DEFAULT_SIZE         2.0f
#define QUAD_DEFAULT_SUBX         2
#define QUAD_DEFAULT_SUBY         2
#define BOX_DEFAULT_SUBY          2
#define BOX_DEFAULT_SUBX          2
#define SPHERE_DEFAULT_SUBX       18
#define SPHERE_DEFAULT_SUBY       18
#define CYLINDER_DEFAULT_SUBX     18
#define CYLINDER_DEFAULT_SUBY     2
#define TORUS_DEFAULT_SUB_INNER   18
#define TORUS_DEFAULT_SUB_OUTER   18
#define ICOSPHERE_DEFAULT_STEPS   0 // Lowest level

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

void mesh_create_quad(mesh *m, uint32_t subx, uint32_t suby, uint32_t mtl, bool invert_normals);
void mesh_create_box(mesh *m, uint32_t subx, uint32_t suby, uint32_t mtl, bool invert_normals);
void mesh_create_uvsphere(mesh *m, float radius, uint32_t subx, uint32_t suby, uint32_t mtl, bool face_normals, bool invert_normals);
void mesh_create_uvcylinder(mesh *m, float radius, float height, uint32_t subx, uint32_t suby, bool caps, uint32_t mtl, bool face_normals, bool invert_normals);
void mesh_create_torus(mesh *m, float inner_radius, float outer_radius, uint32_t sub_inner, uint32_t sub_outer, uint32_t mtl, bool face_normals, bool invert_normals);
void mesh_create_icosphere(mesh *m, uint8_t steps, uint32_t mtl, bool face_normals, bool invert_normals);

#endif
