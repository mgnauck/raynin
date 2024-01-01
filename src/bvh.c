#include "bvh.h"
#include "scn.h"
#include "aabb.h"
#include "log.h"

void bvh_add_node(bvh *b, const scn *s, int32_t start_idx, size_t obj_cnt)
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
  memcpy(&b->node_buf[b->node_cnt++], &n, sizeof(n));
}

void bvh_subdivide_node(bvh *b, const scn *s, uint32_t node_idx)
{
}

bvh *bvh_create(const scn *s)
{
  bvh *b = malloc(sizeof(*b));
  b->node_buf = malloc((2 * s->obj_idx - 1) * sizeof(*b->node_buf));
  b->node_cnt = 0;
 
  bvh_add_node(b, s, 0, s->obj_idx);
  bvh_subdivide_node(b, s, 0);

  return b;
}

void bvh_release(bvh *b)
{
  free(b->node_buf);
  free(b);
}
