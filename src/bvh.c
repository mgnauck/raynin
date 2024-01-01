#include "bvh.h"
#include <float.h>
#include "scn.h"
#include "aabb.h"
#include "log.h"

#define NODE_INTERVAL_COUNT 8

typedef struct split {
  float   cost;
  float   pos;
  uint8_t axis;
} split;

split find_best_cost_interval_split(const scn *s, bvh_node *n)
{
  split best = { .cost = FLT_MAX };

  for(uint8_t axis=0; axis<3; axis++) {
   // TODO 
  }

  return best;
}

bvh_node *bvh_add_node(bvh *b, const scn *s, int32_t start_idx, size_t obj_cnt)
{
  aabb node_aabb = aabb_init();
  for(size_t i=0; i<obj_cnt; i++) {
    aabb obj_aabb = scn_get_obj_aabb(s, start_idx + i);
    log("obj aabb: %f, %f, %f / %f, %f, %f",
        obj_aabb.min.x, obj_aabb.min.y, obj_aabb.min.z,
        obj_aabb.max.x, obj_aabb.max.y, obj_aabb.max.z);
    node_aabb = aabb_combine(node_aabb, obj_aabb);
  }

  log("node aabb: %f, %f, %f / %f, %f, %f",
      node_aabb.min.x, node_aabb.min.y, node_aabb.min.z,
      node_aabb.max.x, node_aabb.max.y, node_aabb.max.z);
  log("start_idx: %d, obj_cnt: %d", start_idx, obj_cnt);
  log("---");

  bvh_node n = { node_aabb.min, start_idx, node_aabb.max, obj_cnt };
  return memcpy(&b->node_buf[b->node_cnt++], &n, sizeof(n));
  //return &b->node_buf[b->node_cnt - 1];
}

void bvh_subdivide_node(bvh *b, const scn *s, bvh_node *n)
{
  // Calculate if we need to split or not
  split split = find_best_cost_interval_split(s, n);
  float no_split_cost = n->obj_cnt * aabb_calc_area((aabb){ n->min, n->max });
  if(no_split_cost < split.cost)
    return;

  // Partition object data into left and right of split pos
  int32_t l = n->start_idx;
  int32_t r = n->start_idx + n->obj_cnt - 1;
  while(l <= r) {
    vec3 center = scn_get_obj_center(s, l);
    if(center.v[split.axis] < split.pos) {
      l++;
    } else {
      // Swap object data left/right
      obj *lo = scn_get_obj(s, l);
      obj *ro = scn_get_obj(s, r);
      obj t = *lo;
      memcpy(lo, ro, sizeof(obj));
      memcpy(ro, &t, sizeof(obj));
      r--;
    }
  }

  // Stop if one side of the l/r partition is empty
  uint32_t left_obj_cnt = l - n->start_idx;
  if(left_obj_cnt == 0 || left_obj_cnt == n->obj_cnt)
    return;

  uint32_t left_child_node_idx = b->node_cnt;

  // Current node is not a leaf. Set index of left child node.
  n->start_idx = left_child_node_idx;
  n->obj_cnt = 0;

  bvh_node *left_child = bvh_add_node(b, s, n->start_idx, left_obj_cnt);
  bvh_node *right_child = bvh_add_node(b, s, l, n->obj_cnt - left_obj_cnt);

  // Recurse
  bvh_subdivide_node(b, s, left_child);
  bvh_subdivide_node(b, s, right_child);
}

bvh *bvh_create(const scn *s)
{
  bvh *b = malloc(sizeof(*b));
  b->node_buf = malloc((2 * s->obj_idx - 1) * sizeof(*b->node_buf));
  b->node_cnt = 0;
 
  bvh_node *n = bvh_add_node(b, s, 0, s->obj_idx);
  bvh_subdivide_node(b, s, n);

  return b;
}

void bvh_release(bvh *b)
{
  free(b->node_buf);
  free(b);
}
