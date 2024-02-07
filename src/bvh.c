#include "bvh.h"
#include <float.h>
#include "buf.h"
#include "mutil.h"
#include "tri.h"
#include "aabb.h"

#define INTERVAL_CNT 16

typedef struct interval {
  aabb    aabb;
  uint32_t  cnt;
} interval;

typedef struct split {
  float   cost;
  float   pos;
  uint8_t axis;
} split;

void bvh_init(bvh *b, uint32_t tri_cnt)
{
  // Would be 2 * tri_cnt - 1 but we skip one node
  b->node_cnt = 0;
  b->nodes = buf_acquire(BVH_NODE, 2 * tri_cnt);
  b->indices = buf_acquire(INDEX, tri_cnt);
}

// Guenther et al: Realtime Ray Tracing on GPU with BVH-based Packet Traversal
// Section Fast BVH Construction
split find_best_cost_interval_split(const bvh *b, bvh_node *n, const tri *tris)
{
  split best = { .cost = FLT_MAX };
  for(uint8_t axis=0; axis<3; axis++) {
    // Calculate bounds of object centers
    float minc = FLT_MAX;
    float maxc = -FLT_MAX;
    for(uint32_t i=0; i<n->obj_cnt; i++) {
      float c = vec3_get(tris[b->indices[n->start_idx + i]].center, axis);
      minc = min(minc, c);
      maxc = max(maxc, c);
    }
    if(fabsf(maxc - minc) < EPSILON)
      continue;
    
    // Initialize empty intervals
    interval intervals[INTERVAL_CNT];
    for(uint32_t i=0; i<INTERVAL_CNT; i++)
      intervals[i] = (interval){ aabb_init(), 0 };

    // Count objects per interval and find their combined bounds
    float delta = INTERVAL_CNT / (maxc - minc);
    for(uint32_t i=0; i<n->obj_cnt; i++) {
      vec3 center = tris[b->indices[n->start_idx + i]].center;
      uint32_t int_idx =
        (uint32_t)min(INTERVAL_CNT - 1, (vec3_get(center, axis) - minc) * delta);
      aabb *int_aabb = &intervals[int_idx].aabb;
      const tri *tri = &tris[b->indices[n->start_idx + i]];
      aabb_grow(int_aabb, tri->v0);
      aabb_grow(int_aabb, tri->v1);
      aabb_grow(int_aabb, tri->v2);
      intervals[int_idx].cnt++;
    }

    // Calculate left/right area and count for each plane separating the intervals
    float areas_l[INTERVAL_CNT - 1];
    float areas_r[INTERVAL_CNT - 1];
    uint32_t cnts_l[INTERVAL_CNT - 1];
    uint32_t cnts_r[INTERVAL_CNT - 1];
    aabb aabb_l = aabb_init();
    aabb aabb_r = aabb_init();
    uint32_t total_cnt_l = 0;
    uint32_t total_cnt_r = 0;
    for(uint32_t i=0; i<INTERVAL_CNT - 1; i++) {
      // From left
      total_cnt_l += intervals[i].cnt;
      cnts_l[i] = total_cnt_l;
      aabb_l = aabb_combine(aabb_l, intervals[i].aabb);
      areas_l[i] = aabb_calc_area(aabb_l);
      // From right
      total_cnt_r += intervals[INTERVAL_CNT - 1 - i].cnt;
      cnts_r[INTERVAL_CNT - 2 - i] = total_cnt_r;
      aabb_r = aabb_combine(aabb_r, intervals[INTERVAL_CNT - 1 - i].aabb);
      areas_r[INTERVAL_CNT - 2 - i] = aabb_calc_area(aabb_r);
    }

    // Find best surface area cost for prepared interval planes
    delta = 1.0f / delta;
    for(uint32_t i=0; i<INTERVAL_CNT - 1; i++) {
      float cost = cnts_l[i] * areas_l[i] + cnts_r[i] * areas_r[i];
      if(cost < best.cost) {
        best.cost = cost;
        best.axis = axis;
        best.pos = minc + (i + 1) * delta;
      }
    }
  }

  return best;
}

void update_node_bounds(const bvh *b, bvh_node *n, const tri *tris)
{
  n->min = (vec3){ FLT_MAX, FLT_MAX, FLT_MAX };
  n->max = (vec3){ -FLT_MAX, -FLT_MAX, -FLT_MAX };
  for(uint32_t i=0; i<n->obj_cnt; i++) {
    const tri *t = &tris[b->indices[n->start_idx + i]];
    n->min = vec3_min(n->min, t->v0);
    n->min = vec3_min(n->min, t->v1);
    n->min = vec3_min(n->min, t->v2);
    n->max = vec3_max(n->max, t->v0);
    n->max = vec3_max(n->max, t->v1);
    n->max = vec3_max(n->max, t->v2);
  }
}

void subdivide_node(bvh *b, bvh_node *n, const tri *tris)
{
  // Calculate if we need to split or not
  split split = find_best_cost_interval_split(b, n, tris);
  float no_split_cost = n->obj_cnt * aabb_calc_area((aabb){ n->min, n->max });
  if(no_split_cost <= split.cost)
    return;

  // Partition object data into left and right of split pos
  int32_t l = n->start_idx;
  int32_t r = n->start_idx + n->obj_cnt - 1;
  while(l <= r) {
    if(vec3_get(tris[b->indices[l]].center, split.axis) < split.pos) {
      l++;
    } else {
      // Swap object index left/right
      uint32_t t = b->indices[l];
      b->indices[l] = b->indices[r];
      b->indices[r] = t;
      r--;
    }
  }

  // Stop if one side of the l/r partition is empty
  uint32_t left_obj_cnt = l - n->start_idx;
  if(left_obj_cnt == 0 || left_obj_cnt == n->obj_cnt)
    return;

  // Init child nodes
  bvh_node *left_child = &b->nodes[b->node_cnt++];
  left_child->start_idx = n->start_idx;
  left_child->obj_cnt = left_obj_cnt;

  update_node_bounds(b, left_child, tris);

  bvh_node *right_child = &b->nodes[b->node_cnt++];
  right_child->start_idx = l;
  right_child->obj_cnt = n->obj_cnt - left_obj_cnt;

  update_node_bounds(b, right_child, tris);

  // Update current node with child link
  n->start_idx = b->node_cnt - 2; // Right child implicitly + 1
  n->obj_cnt = 0; // No leaf

  subdivide_node(b, left_child, tris);
  subdivide_node(b, right_child, tris);
}

void bvh_build(bvh *b, const tri *tris, uint32_t tri_cnt)
{
  b->node_cnt = 0;

  for(uint32_t i=0; i<tri_cnt; i++)
    b->indices[i] = i;

  bvh_node *root = &b->nodes[b->node_cnt++];
  root->start_idx = 0;
  root->obj_cnt = tri_cnt;

  // Skip node 1 to have children aligned in mem
  b->node_cnt++;

  update_node_bounds(b, root, tris);
  subdivide_node(b, root, tris);
}

void bvh_update(bvh *b, const tri *tris)
{
  for(int32_t i=b->node_cnt; i>=0; i--) {
    if(i == 1)
      // Skip node 1 due to mem alignment
      continue;
    bvh_node *n = &b->nodes[i];
    if(n->obj_cnt > 0) {
      // Leaf with objects
      update_node_bounds(b, n, tris);
    } else {
      // Interior node. Just update bounds as per child bounds.
      bvh_node *l = &b->nodes[n->start_idx];
      bvh_node *r = &b->nodes[n->start_idx + 1];
      n->min = vec3_min(l->min, r->min);
      n->max = vec3_max(l->max, r->max);
    }
  }
}
