#ifndef INST_H
#define INST_H

#include <stdint.h>
#include "../util/mat4.h"
#include "../util/aabb.h"

#define INST_ID_MASK      0xffff
#define MTL_OVERRIDE_BIT  0x80000000
#define INST_DATA_MASK    0x7fffffff

typedef enum inst_state {
  IS_DISABLED     = 1,
  IS_INVISIBLE    = 2,
  IS_TRANS_DIRTY  = 4,
  IS_MTL_DIRTY    = 8,
  IS_EMISSIVE     = 16,
  IS_WAS_EMISSIVE = 32
} inst_state;

// Additional CPU side data for instances
typedef struct inst_info {
  mat4        transform;
  mat4        inv_transform;
  aabb        box;
  uint16_t    mesh_id;
  uint32_t    ltri_ofs;
  uint32_t    ltri_cnt;
  uint32_t    state;
} inst_info;

// inst.data:
// Bit 32 indicates if the material override is active (MTL_OVERRIDE_BIT).
// For mesh types, bits 0-31 is an offset into tris or 2 * ofs into blas_nodes

typedef struct inst {
  float       inv_transform[12];
  uint32_t    id;     // (mtl override id << 16) | (inst id & 0xffff)
  uint32_t    data;   // See notes above
  uint32_t    pad0;
  uint32_t    pad1;
} inst;

#endif
