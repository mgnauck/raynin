#include "bvh.h"
#include <float.h>
#include "../scene/inst.h"
#include "../scene/tri.h"
#include "../util/aabb.h"

static uint32_t find_best_node(bvhnode *nodes,
                               uint32_t idx,
                               uint32_t *node_indices,
                               uint32_t node_indices_cnt)
{
  float     best_cost = FLT_MAX;
  uint32_t  best_idx;

  uint32_t curr_idx = node_indices[idx];
  vec3 curr_min = nodes[curr_idx].min;
  vec3 curr_max = nodes[curr_idx].max;

  // Find smallest combined aabb of current node and any other node
  for(uint32_t i=0; i<node_indices_cnt; i++) {
    if(idx != i) {
      uint32_t other_idx = node_indices[i];
      vec3 mi = vec3_min(curr_min, nodes[other_idx].min);
      vec3 ma = vec3_max(curr_max, nodes[other_idx].max);
      float cost = aabb_calc_area(&(aabb){ mi, ma });
      if(cost < best_cost) {
        best_cost = cost;
        best_idx = i;
      }
    }
  }

  return best_idx;
}

// Walter et al: Fast Agglomerative Clustering for Rendering
uint32_t cluster_nodes(bvhnode *nodes, uint32_t node_idx,
                       uint32_t *node_indices, uint32_t node_indices_cnt)
{
  uint32_t a = 0;
  uint32_t b = find_best_node(nodes, a, node_indices, node_indices_cnt);
  while(node_indices_cnt > 1) {
    uint32_t c = find_best_node(nodes, b, node_indices, node_indices_cnt);
    if(a == c) {
      uint32_t idx_a = node_indices[a];
      uint32_t idx_b = node_indices[b];

      bvhnode *node_a = &nodes[idx_a];
      bvhnode *node_b = &nodes[idx_b];

      // Claim new node which is the combination of node A and B
      bvhnode *new_node = &nodes[node_idx];
      new_node->min = vec3_min(node_a->min, node_b->min);
      new_node->max = vec3_max(node_a->max, node_b->max);
      // Each child node index gets 16 bits
      new_node->children = (idx_b << 16) | idx_a;

      // Replace node A with newly created combined node
      node_indices[a] = node_idx--;

      // Remove node B by replacing its slot with last node
      node_indices[b] = node_indices[--node_indices_cnt];

      // Restart the loop for remaining nodes
      b = find_best_node(nodes, a, node_indices, node_indices_cnt);
    } else {
      // The best match B we found for A had itself a better match in C, thus
      // A and B are not best matches and we continue searching with B and C.
      a = b;
      b = c;
    }
  }

  // Index of root node - 1
  return node_idx;
}

void blas_build(bvhnode *nodes, const tri *tris, uint32_t tri_cnt)
{
  uint32_t node_indices[tri_cnt];
  uint32_t node_indices_cnt = 0;

  // We will have 2 * tri_cnt - 1 nodes
  // Nodes will be placed beginning at the back of the array
  uint32_t node_idx = 2 * tri_cnt - 2;

  // Construct leaf node for each tri
  for(uint32_t i=0; i<tri_cnt; i++) {
    const tri *t = &tris[i];

    aabb a = aabb_init();
    aabb_grow(&a, t->v0);
    aabb_grow(&a, t->v1);
    aabb_grow(&a, t->v2);

    bvhnode *n = &nodes[node_idx];
    n->min = a.min;
    n->max = a.max;
    // 2x 16 bits for left and right child
    // Interior nodes will have at least one child > 0
    n->children = 0;
    n->idx = i;

    node_indices[node_indices_cnt++] = node_idx--;
  }

  cluster_nodes(nodes, node_idx, node_indices, node_indices_cnt);
}

void tlas_build(bvhnode *nodes, const inst_info *instances, uint32_t inst_cnt)
{
  uint32_t node_indices[inst_cnt];
  uint32_t node_indices_cnt = 0;

  // We will have 2 * inst_cnt - 1 nodes
  // Nodes will be placed beginning at the back of the array
  uint32_t node_idx = 2 * inst_cnt - 2;

  // Construct a leaf node for each instance
  for(uint32_t i = 0; i < inst_cnt; i++) {
    if((instances[i].state & IS_DISABLED) == 0) {
      bvhnode *n = &nodes[node_idx];
      n->min = instances[i].box.min;
      n->max = instances[i].box.max;
      // 2x 16 bits for left and right child
      // Interior nodes will have at least one child > 0
      n->children = 0;
      n->idx = i;

      node_indices[node_indices_cnt++] = node_idx--;
    }
  }

  node_idx =
      cluster_nodes(nodes, node_idx, node_indices, node_indices_cnt);

  if(node_idx + 1 > 0)
    // There were gaps (not all assumed leaf nodes were populated)
    // Move root node to front
    nodes[0] = nodes[node_idx + 1];
}
