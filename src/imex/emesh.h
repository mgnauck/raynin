#ifndef EMESH_H
#define EMESH_H

#include <stdbool.h>
#include <stdint.h>

typedef struct vec3 vec3;

typedef enum obj_type {
  OT_QUAD = 0,
  OT_BOX,
  OT_SPHERE,
  OT_CYLINDER,
  OT_TORUS,
  OT_MESH,
} obj_type;

typedef struct vec3 vec3;

typedef struct emesh {
  uint16_t mtl_id; // 0xffff = not set (loaded meshes with mtl per tri)
  uint8_t type;    // (((no_caps << 2) | (invert_nrms << 1) | (face_nrms & 0x1))
                   // << 4) | (obj_type & 0xf)
  uint8_t subx;    // !OT_MESH
  uint8_t suby;    // !OT_MESH
  float in_radius; // !OT_MESH
  uint16_t vertex_cnt; // OT_MESH only (all subsequent)
  vec3 *vertices;
  vec3 *normals;
  uint16_t index_cnt; // Limit to 16 bit
  uint16_t *indices;
  uint32_t vertices_ofs; // Modified and used by export
  uint32_t normals_ofs;  // Modified and used by export
  uint32_t indices_ofs;  // Modified and used by export
  uint8_t share_id;      // NOT written (just for identification)
} emesh;

void emesh_init(emesh *m, uint32_t vertex_cnt, uint16_t index_cnt);
void emesh_release(emesh *m);

uint32_t emesh_calc_size(const emesh *m);

uint8_t *emesh_write_primitive(uint8_t *dst, const emesh *m);

#endif
