#ifndef BVH_H
#define BVH_H

#include <stddef.h>
#include <stdint.h>
#include "vec3.h"

typedef struct tri tri;

typedef struct bvh_node {
  vec3      min;
  size_t    start_idx; // obj start or node index
  vec3      max;
  size_t    obj_cnt;
} bvh_node;

typedef struct bvh {
  size_t    node_cnt;
  bvh_node  *nodes;
  size_t    *indices;
} bvh;

void  bvh_init(bvh *b, size_t tri_cnt);
void  bvh_build(bvh *b, const tri *tris, size_t tri_cnt);
void  bvh_update(bvh *b, const tri *tris);

#endif
