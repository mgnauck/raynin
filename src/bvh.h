#ifndef BVH_H
#define BVH_H

#include <stddef.h>
#include <stdint.h>
#include "vec3.h"

typedef struct scn scn;

typedef struct bvh_node {
  vec3    min;
  size_t  start_idx; // obj start or node index
  vec3    max;
  size_t  obj_cnt;
} bvh_node;

typedef struct bvh {
  size_t    node_cnt;
  bvh_node  *nodes;
  size_t    *indices;
} bvh;

bvh   *bvh_init(size_t obj_cnt);
void  bvh_create(bvh *b, const scn *s);
void  bvh_refit(bvh *b, const scn *s);
void  bvh_release(bvh *b);

#endif
