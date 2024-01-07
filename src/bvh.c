#include "bvh.h"
#include <float.h>
#include "sutil.h"
#include "mutil.h"
#include "scn.h"
#include "obj.h"
#include "aabb.h"

#define INTERVAL_CNT 8

typedef struct interval {
  aabb    aabb;
  size_t  cnt;
} interval;

typedef struct split {
  float   cost;
  float   pos;
  uint8_t axis;
} split;

split find_best_cost_interval_split(const scn *s, bvh_node *n)
{
  split best = { .cost = FLT_MAX };
  for(uint8_t axis=0; axis<3; axis++) {
    // Calculate bounds of object centers
    float minc = FLT_MAX;
    float maxc = -FLT_MAX;
    for(size_t i=0; i<n->obj_cnt; i++) {
      float c = scn_get_obj_center(s, n->start_idx + i).v[axis];
      minc = min(minc, c);
      maxc = max(maxc, c);
    }
    if(fabsf(maxc - minc) < EPSILON)
      continue;
    
    // Initialize empty intervals
    interval intervals[INTERVAL_CNT];
    for(size_t i=0; i<INTERVAL_CNT; i++)
      intervals[i] = (interval){ aabb_init(), 0 };

    // Count objects per interval and find their combined bounds
    float delta = INTERVAL_CNT / (maxc - minc);
    for(size_t i=0; i<n->obj_cnt; i++) {
      vec3 center = scn_get_obj_center(s, n->start_idx + i);
      uint32_t int_idx =
        (uint32_t)min(INTERVAL_CNT - 1, (center.v[axis] - minc) * delta);
      intervals[int_idx].aabb = aabb_combine(
          intervals[int_idx].aabb, scn_get_obj_aabb(s, n->start_idx + i));
      intervals[int_idx].cnt++;
    }

    // Calculate left/right area and count for each plane separating the intervals
    float areas_l[INTERVAL_CNT - 1];
    float areas_r[INTERVAL_CNT - 1];
    size_t cnts_l[INTERVAL_CNT - 1];
    size_t cnts_r[INTERVAL_CNT - 1];
    aabb aabb_l = aabb_init();
    aabb aabb_r = aabb_init();
    size_t total_cnt_l = 0;
    size_t total_cnt_r = 0;
    for(size_t i=0; i<INTERVAL_CNT - 1; i++) {
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
    for(size_t i=0; i<INTERVAL_CNT - 1; i++) {
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

bvh_node *bvh_add_node(bvh *b, const scn *s, int32_t start_idx, size_t obj_cnt)
{
  aabb node_aabb = aabb_init();
  for(size_t i=0; i<obj_cnt; i++) {
    aabb obj_aabb = scn_get_obj_aabb(s, start_idx + i);
    /*log("obj aabb: %f, %f, %f / %f, %f, %f",
        obj_aabb.min.x, obj_aabb.min.y, obj_aabb.min.z,
        obj_aabb.max.x, obj_aabb.max.y, obj_aabb.max.z);*/
    node_aabb = aabb_combine(node_aabb, obj_aabb);
  }

  /*log("node aabb: %f, %f, %f / %f, %f, %f",
      node_aabb.min.x, node_aabb.min.y, node_aabb.min.z,
      node_aabb.max.x, node_aabb.max.y, node_aabb.max.z);
  log("start_idx: %d, obj_cnt: %d", start_idx, obj_cnt);
  log("---");*/

  return memcpy(&b->node_buf[b->node_cnt++],
      &(bvh_node){ node_aabb.min, start_idx, node_aabb.max, obj_cnt },
      sizeof(bvh_node));
}

void bvh_subdivide_node(bvh *b, const scn *s, bvh_node *n)
{
  // Calculate if we need to split or not
  split split = find_best_cost_interval_split(s, n);
  float no_split_cost = n->obj_cnt * aabb_calc_area((aabb){ n->min, n->max });
  if(no_split_cost <= split.cost)
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

  bvh_node *left_child = bvh_add_node(b, s, n->start_idx, left_obj_cnt);
  bvh_node *right_child = bvh_add_node(b, s, l, n->obj_cnt - left_obj_cnt);

  // Current node is not a leaf. Set index of left child node.
  n->start_idx = left_child_node_idx;
  n->obj_cnt = 0;

  // Recurse
  bvh_subdivide_node(b, s, left_child);
  bvh_subdivide_node(b, s, right_child);
}

bvh *bvh_create(const scn *s)
{
  bvh *b = malloc(sizeof(*b));
  b->node_buf = malloc((2 * s->obj_cnt - 1) * sizeof(*b->node_buf));
  b->node_cnt = 0;
 
  bvh_node *n = bvh_add_node(b, s, 0, s->obj_cnt);
  bvh_subdivide_node(b, s, n);

  return b;
}

void bvh_release(bvh *b)
{
  free(b->node_buf);
  free(b);
}
