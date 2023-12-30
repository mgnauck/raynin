#ifndef BVH_H
#define BVH_H

#include <stddef.h>
#include "util.h"
#include "vec3.h"

typedef struct scene scene;
typedef struct bvh_node bvh_node;

typedef struct bvh {
  bvh_node *nodes;
  size_t node_cnt;
} bvh;

bvh *bvh_create(const scene *scene);

#endif
