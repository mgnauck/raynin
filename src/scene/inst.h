#ifndef INST_H
#define INST_H

#include <stdint.h>
#include "../util/mat4.h"

#define INST_ID_MASK      0xffff
#define SHAPE_TYPE_BIT    0x40000000
#define MESH_SHAPE_MASK   0x3fffffff
#define MTL_OVERRIDE_BIT  0x80000000
#define INST_DATA_MASK    0x7fffffff

typedef enum inst_state {
  IS_DISABLED     = 1,
  IS_TRANS_DIRTY  = 2,
  IS_MTL_DIRTY    = 4,
  IS_EMISSIVE     = 8,
  IS_WAS_EMISSIVE = 16
} inst_state;

typedef struct inst_info {
  mat4        transform;
  uint32_t    mesh_shape;
  uint32_t    ltri_ofs;
  uint32_t    ltri_cnt;
  uint32_t    state;
} inst_info;

// inst.data:
// If bit 31 is set, then the instance is a shape (SHAPE_TYPE_BIT).
// If bit 31 is not set, then it is a mesh.
// Bit 32 indicates if the material override is active (MTL_OVERRIDE_BIT).
// (For shape types, mtl override is mandatory.)
// For mesh types, bits 0-30 is an offset into
// tris/indices or 2 * ofs into bvh_nodes
// In case of native build, bits 0-30 is simply the mesh id
// For shape types, bits 0-30 indicate the shape type,
// i.e. unitsphere, unitcylinder, unitbox

typedef struct inst {
  float       inv_transform[12];
  vec3        min;
  uint32_t    id;     // (mtl override id << 16) | (inst id & 0xffff)
  vec3        max;
  uint32_t    data;   // See notes above
} inst;

#endif
