#include "bvh.h"
#include <stdint.h>
#include "scene.h"
#include "aabb.h"

typedef struct bvh_node {
  vec3 min;
  uint32_t start_idx;
  vec3 max;
  uint32_t obj_cnt;
} bvh_node;

void bvh_add_node(bvh *b, const scene *s, int32_t start_idx, size_t obj_cnt)
{
  aabb node_aabb = aabb_init();
  for(size_t i=0; i<obj_cnt; i++) {
    aabb obj_aabb = obj_get_aabb(s, start_idx + i); // TODO
    node_aabb = aabb_combine(node_aabb, obj_aabb);
  }

  bvh_node n = { node_aabb.min, start_idx, node_aabb.max, obj_cnt };
  memcpy(&b->nodes[b->node_cnt++], &n, sizeof(n));
}

void bvh_subdivide_node(bvh *b, const scene *s, uint32_t node_idx)
{
}

bvh *bvh_create(const scene *s)
{
  bvh *b = malloc(sizeof(*b));
  // TODO Alloc max node count
  // b->nodes = malloc()
  b->node_cnt = 0;
 
  bvh_add_node(b, s, 0, s->object_line_count);
  bvh_subdivide_node(b, s, 0);

  return b;
}
