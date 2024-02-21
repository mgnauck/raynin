#ifndef INST_H
#define INST_H

#include <stdint.h>
#include "mat4.h"

typedef struct bvh bvh;
typedef struct mat mat;

typedef struct inst {
  mat4      transform;
  mat4      inv_transform;
  vec3      min;
  uint32_t  id;   // (mat id << 16) | (inst id & 0xffff)
  vec3      max;
  uint32_t  ofs;  // ofs into tris/indices and 2 * ofs into bvh_nodes
} inst;

void inst_create(inst *inst, uint32_t inst_idx, const mat4 transform,
    const bvh *bvh, const mat *mat);

void inst_transform(inst *inst, const bvh *bvh, const mat4 transform);

#endif
