#ifndef BVH_H
#define BVH_H

#include <stdint.h>

#include "../util/vec3.h"

typedef struct inst_info inst_info;
typedef struct tri tri;

// BLAS and TLAS node
typedef struct node {
  vec3 lmin;
  int32_t left;
  vec3 lmax;
  uint32_t pad0;
  vec3 rmin;
  int32_t right;
  vec3 rmax;
  uint32_t pad1;
} node;

void blas_build(node *nodes, const tri *tris, uint32_t tri_cnt);
void tlas_build(node *nodes, const inst_info *instances, uint32_t inst_cnt);

#endif
