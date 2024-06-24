#ifndef BVH_H
#define BVH_H

#include <stdint.h>
#include "../util/vec3.h"

typedef struct mesh mesh;

typedef struct bvh_node {
  vec3      min;
  uint32_t  start_idx; // obj start or node index
  vec3      max;
  uint32_t  obj_cnt;
} bvh_node;

typedef struct bvh {
  uint32_t  node_cnt;
  bvh_node  *nodes;
  uint32_t  *indices;
  mesh      *mesh;
} bvh;

void  bvh_init(bvh *b, mesh *m);
void  bvh_build(bvh *b);
void  bvh_update(bvh *b);

#endif
