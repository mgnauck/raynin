#ifndef EMESH_H
#define EMESH_H

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

typedef struct emesh {
  uint16_t  mtl_id;       // 0xffff = not set (loaded meshes with mtl per tri)
  uint8_t   type;         // obj_type
  uint8_t   subx;
  uint8_t   suby;
  uint8_t   flags;        // (no_caps << 2) | (invert_nrms << 1) | (face_nrms & 0x1)
  float     in_radius;
  uint32_t  vertex_cnt;   // OT_MESH only
  vec3      *vertices;
  vec3      *normals;
  uint16_t  index_cnt;    // Limit to 16 bit
  uint16_t  *indices;
  uint32_t  vertices_ofs; // Modified and used by export
  uint32_t  normals_ofs;  // Modified and used by export
  uint32_t  indices_ofs;  // Modified and used by export
  uint8_t   share_id;     // Does not get written, just for identification
} emesh;

void      emesh_init(emesh *m, uint32_t vertex_cnt, uint16_t index_cnt);
void      emesh_release(emesh *m);

uint32_t  emesh_calc_mesh_data_size(emesh const *m);
uint32_t  emesh_calc_size(emesh const *m);

uint8_t   *emesh_write_primitive(uint8_t *dst, emesh const *m);

#endif
