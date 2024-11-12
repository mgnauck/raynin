#ifndef BVH_H
#define BVH_H

#include <stdint.h>
#include "../util/vec3.h"

typedef struct inst_info inst_info;
typedef struct tri tri;

typedef struct bvhnode {
  vec3      min;
  uint32_t  children; // 2x 16 bit child node id
  vec3      max;
  uint32_t  idx;      // Index only specified at leaf node
} bvhnode;

void blas_build(bvhnode *nodes, const tri *tris, uint32_t tri_cnt);
void tlas_build(bvhnode *nodes, const inst_info *instances, uint32_t inst_cnt);

#endif
