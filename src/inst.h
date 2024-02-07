#ifndef INST_H
#define INST_H

#include <stdint.h>
#include "mat4.h"
#include "mat.h"

typedef struct mesh mesh;
typedef struct bvh bvh;

typedef struct inst {
  mat4      transform;
  mat4      inv_transform;
  vec3      min;
  uint32_t  id;   // (mat type << 28) | (mat id << 16) | (inst id & 0xffff)
  vec3      max;
  uint32_t  ofs;  // ofs into tris/tris_data/indices and 2*ofs into bvh_nodes
} inst;

void inst_create(inst *inst, uint32_t inst_idx, const mat4 transform,
    const mesh *mesh, const bvh *bvh, uint32_t mat_type, const mat *mat);

#endif
