#ifndef INST_H
#define INST_H

#include <stdint.h>
#include "mat4.h"

// data:
// If bit 31 is set, then the instance is a shape.
// If bit 31 is not set, then it is a mesh.
// Bit 32 indicates if the material override is active
// (For shape type, mtl override is mandatory)
// For mesh types, bits 0-30 is an offset into
// tris/indices or 2 * ofs into bvh_nodes
// In case of native build, bits 0-30 is simply the mesh id
// For shape types, bits 0-30 indicate the shape type,
// i.e. unitsphere, unitcylinder, unitbox

typedef struct inst {
  mat4        transform;
  mat4        inv_transform;
  vec3        min;
  uint32_t    id;     // (mtl override id << 16) | (inst id & 0xffff)
  vec3        max;
  uint32_t    data;   // See notes above
} inst;

#endif
