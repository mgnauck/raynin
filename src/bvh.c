#include "bvh.h"
#include <float.h>
#include "sutil.h"
#include "mutil.h"
#include "scn.h"
#include "obj.h"
#include "shape.h"
#include "log.h"

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

aabb get_obj_aabb(const bvh * b, const scn *s, size_t idx)
{
  obj *o = scn_get_obj(s, b->indices[idx]);
  switch(o->shape_type) {
    case SPHERE:
      return sphere_get_aabb((sphere *)scn_get_shape(s, o->shape_ofs));
      break;
    case QUAD:
      return quad_get_aabb((quad *)scn_get_shape(s, o->shape_ofs));
      break;
    default:
      // Unknown or unsupported shape
      // TODO Log/alert
      return aabb_init();
  }
}

vec3 get_obj_center(const bvh *b, const scn *s, size_t idx)
{
  obj *o = scn_get_obj(s, b->indices[idx]);
  switch(o->shape_type) {
    case SPHERE:
      return ((sphere *)scn_get_shape(s, o->shape_ofs))->center;
      break;
    case QUAD:
      return quad_get_center((quad *)scn_get_shape(s, o->shape_ofs));
      break;
   default:
      // Unknown or unsupported shape
      // TODO Log/alert
      return (vec3){ 0.0f, 0.0f, 0.0f };
  }
}

split find_best_cost_interval_split(const bvh *b, const scn *s, bvh_node *n)
{
  split best = { .cost = FLT_MAX };
  for(uint8_t axis=0; axis<3; axis++) {
    // Calculate bounds of object centers
    float minc = FLT_MAX;
    float maxc = -FLT_MAX;
    for(size_t i=0; i<n->obj_cnt; i++) {
      float c = vec3_get(get_obj_center(b, s, n->start_idx + i), axis);
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
      vec3 center = get_obj_center(b, s, n->start_idx + i);
      size_t int_idx =
        (size_t)min(INTERVAL_CNT - 1, (vec3_get(center, axis) - minc) * delta);
      intervals[int_idx].aabb = aabb_combine(
          intervals[int_idx].aabb, get_obj_aabb(b, s, n->start_idx + i));
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

void update_node_bounds(const bvh *b, const scn *s, bvh_node *n)
{
  n->min = (vec3){ FLT_MAX, FLT_MAX, FLT_MAX };
  n->max = (vec3){ -FLT_MAX, -FLT_MAX, -FLT_MAX };
  for(size_t i=0; i<n->obj_cnt; i++) {
    aabb obj_aabb = get_obj_aabb(b, s, n->start_idx + i);
    n->min = vec3_min(n->min, obj_aabb.min);
    n->max = vec3_max(n->max, obj_aabb.max);
  }
}

void subdivide_node(bvh *b, const scn *s, bvh_node *n)
{
  // Calculate if we need to split or not
  split split = find_best_cost_interval_split(b, s, n);
  float no_split_cost = n->obj_cnt * aabb_calc_area((aabb){ n->min, n->max });
  if(no_split_cost <= split.cost)
    return;

  // Partition object data into left and right of split pos
  int32_t l = n->start_idx;
  int32_t r = n->start_idx + n->obj_cnt - 1;
  while(l <= r) {
    vec3 center = get_obj_center(b, s, l);
    if(vec3_get(center, split.axis) < split.pos) {
      l++;
    } else {
      // Swap object index left/right
      size_t t = b->indices[l];
      b->indices[l] = b->indices[r];
      b->indices[r] = t;
      r--;
    }
  }

  // Stop if one side of the l/r partition is empty
  size_t left_obj_cnt = l - n->start_idx;
  if(left_obj_cnt == 0 || left_obj_cnt == n->obj_cnt)
    return;

  // Init child nodes
  bvh_node *left_child = &b->nodes[b->node_cnt++];
  left_child->start_idx = n->start_idx;
  left_child->obj_cnt = left_obj_cnt;

  update_node_bounds(b, s, left_child);

  bvh_node *right_child = &b->nodes[b->node_cnt++];
  right_child->start_idx = l;
  right_child->obj_cnt = n->obj_cnt - left_obj_cnt;

  update_node_bounds(b, s, right_child);

  // Update current node with child link
  n->start_idx = b->node_cnt - 2; // Right child implicitly + 1
  n->obj_cnt = 0; // No leaf

  subdivide_node(b, s, left_child);
  subdivide_node(b, s, right_child);
}

bvh *bvh_create(const scn *s)
{
  bvh *b = malloc(sizeof(*b));
  b->nodes = malloc((2 * s->obj_cnt - 1) * sizeof(*b->nodes));
  b->node_cnt = 0;
  b->indices = malloc(s->obj_cnt * sizeof(*b->indices));

  for(size_t i=0; i<s->obj_cnt; i++)
    b->indices[i] = i;

  bvh_node *root = &b->nodes[b->node_cnt++];
  root->start_idx = 0;
  root->obj_cnt = s->obj_cnt;

  update_node_bounds(b, s, root);

  subdivide_node(b, s, root);

  return b;
}

void bvh_refit(bvh *b, const scn *s)
{
  for(size_t i=b->node_cnt - 1; i>=0; i--) {
    bvh_node *n = &b->nodes[i];
    if(n->obj_cnt > 0) {
      // Leaf with objects
      update_node_bounds(b, s, n);
    } else {
      // Interior node. Just update bounds as per child bounds.
      bvh_node *l = &b->nodes[n->start_idx];
      bvh_node *r = &b->nodes[n->start_idx + 1];
      n->min = vec3_min(l->min, r->min);
      n->max = vec3_max(l->max, r->max);
    }
  }
}

void bvh_release(bvh *b)
{
  free(b->indices);
  free(b->nodes);
  free(b);
}
