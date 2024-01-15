#ifndef BVH_H
#define BVH_H

#include <stddef.h>
#include <stdint.h>
#include "vec3.h"

typedef struct mesh mesh;

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
  mesh      *mesh;
} bvh;

bvh   *bvh_init(mesh *m);
void  bvh_create(bvh *b);
void  bvh_refit(bvh *b);
void  bvh_release(bvh *b);

#endif
