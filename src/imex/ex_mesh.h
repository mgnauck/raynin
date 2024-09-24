#ifndef EX_MESH_H
#define EX_MESH_H

#include <stdint.h>
#include <stdbool.h>

typedef struct vec3 vec3;

typedef enum obj_type {
  OT_QUAD,
  OT_BOX,
  OT_SPHERE,
  OT_CYLINDER,
  OT_TORUS,
  OT_MESH,
} obj_type;

typedef struct vec3 vec3;

typedef struct ex_mesh {
  uint16_t  mtl_id;       // 0xffff = not set (loaded meshes with mtl per tri)
  uint8_t   type;         // obj_type
  uint8_t   subx;
  uint8_t   suby;
  uint8_t   share_id;
  bool      face_nrms;
  bool      invert_nrms;
  bool      no_caps;
  float     in_radius;
  uint32_t  vertex_cnt;     // OT_MESH only
  vec3      *vertices;
  vec3      *normals;
  uint16_t  index_cnt;     // Limit to 16 bit
  uint16_t  *indices;
} ex_mesh;

void ex_mesh_init(ex_mesh *m, uint32_t vertex_cnt, uint16_t index_cnt);
void ex_mesh_release(ex_mesh *m);

#endif
