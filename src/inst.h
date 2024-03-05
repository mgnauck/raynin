#ifndef INST_H
#define INST_H

#include <stdint.h>
#include "mat4.h"

typedef struct inst {
  mat4        transform;
  mat4        inv_transform;
  vec3        min;
  uint32_t    id;   // (mtl override id << 16) | (inst id & 0xffff)
  vec3        max;
  uint32_t    ofs;  // Bit 0-31: ofs into tris/indices, 2 * ofs into bvh_nodes
} inst;             // Bit 32: indicates if material override is active
                    // In native build, bit 0-31: mesh id

#endif
