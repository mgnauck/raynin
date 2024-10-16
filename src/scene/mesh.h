#ifndef MESH_H
#define MESH_H

#include <stdbool.h>
#include <stdint.h>

#define QUAD_DEFAULT_SUBX       2
#define QUAD_DEFAULT_SUBY       2
#define BOX_DEFAULT_SUBY        2
#define BOX_DEFAULT_SUBX        2
#define SPHERE_DEFAULT_SUBX     36
#define SPHERE_DEFAULT_SUBY     36
#define CYLINDER_DEFAULT_SUBX   36
#define CYLINDER_DEFAULT_SUBY   2
#define TORUS_DEFAULT_SUB_INNER 36
#define TORUS_DEFAULT_SUB_OUTER 36

#define QUAD_DEFAULT_SIZE          2.0f
#define TORUS_DEFAULT_INNER_RADIUS 0.25f
#define TORUS_DEFAULT_OUTER_RADIUS 1.0f

typedef struct vec3 vec3;
typedef struct tri tri;
typedef struct tri_nrm tri_nrm;

typedef struct mesh {
  uint32_t tri_cnt;
  tri *tris;
  tri_nrm *tri_nrms;
  bool is_emissive;
  uint32_t ofs;
} mesh;

void mesh_init(mesh *m, uint32_t tri_cnt);
void mesh_release(mesh *m);

void mesh_create_quad(mesh *m, uint8_t subx, uint8_t suby, uint16_t mtl,
                      bool invert_normals);
void mesh_create_box(mesh *m, uint8_t subx, uint8_t suby, uint16_t mtl,
                     bool invert_normals);
void mesh_create_sphere(mesh *m, float radius, uint8_t subx, uint8_t suby,
                        uint16_t mtl, bool face_normals, bool invert_normals);
void mesh_create_cylinder(mesh *m, float radius, float height, uint8_t subx,
                          uint8_t suby, bool caps, uint16_t mtl,
                          bool face_normals, bool invert_normals);
void mesh_create_torus(mesh *m, float inner_radius, float outer_radius,
                       uint8_t sub_inner, uint8_t sub_outer, uint16_t mtl,
                       bool face_normals, bool invert_normals);

#endif
