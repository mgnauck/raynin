#ifndef BVH_H
#define BVH_H

#include <stddef.h>
#include <stdint.h>
#include "vec3.h"

typedef struct scn scn;

typedef struct bvh_node {
  vec3      min;
  uint32_t  start_idx; // obj start or node index
  vec3      max;
  uint32_t  obj_cnt;
} bvh_node;

typedef struct bvh {
  bvh_node  *nodes;
  size_t    node_cnt;
} bvh;

bvh   *bvh_create(const scn *s);
void  bvh_release(bvh *b);

#endif
