#ifndef BVH_H
#define BVH_H

#include <stdint.h>
#include "../util/vec3.h"

typedef struct inst_info inst_info;
typedef struct ltri ltri;
typedef struct tri tri;

// BLAS and TLAS node
typedef struct node {
  vec3      min;
  uint32_t  children;     // 2x 16 bit child node id
  vec3      max;
  uint32_t  idx;          // Only set at leaf nodes
} node;

// Light node
typedef struct lnode {
  vec3      min;
  uint32_t  children;     // 2x 16 bit child node id
  vec3      max;
  uint32_t  idx;          // 16 bit parent node id, 16 bit light id (= ltri id for now)
  vec3      nrm;
  float     intensity;    // Sum of emission terms
} lnode;

void blas_build(node *nodes, const tri *tris, uint32_t tri_cnt);
void tlas_build(node *nodes, const inst_info *instances, uint32_t inst_cnt);
void lighttree_build(lnode *nodes, const ltri *ltris, uint32_t ltri_cnt);

#endif
