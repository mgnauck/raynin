#include "bvh.h"

#include <float.h>

#include "../scene/inst.h"
#include "../scene/tri.h"
#include "../util/aabb.h"

static uint32_t find_best_node(uint32_t idx, aabb *aabbs,
                               uint32_t *node_indices,
                               uint32_t node_indices_cnt)
{
  float best_cost = FLT_MAX;
  uint32_t best_idx;

  aabb *a = &aabbs[node_indices[idx]];

  // Find smallest combined aabb of current node and any other node
  for(uint32_t i = 0; i < node_indices_cnt; i++) {
    if(idx != i) {
      aabb c = aabb_combine(a, &aabbs[node_indices[i]]);
      vec3 d = vec3_sub(c.max, c.min);
      float cost = d.x * d.y + d.y * d.z + d.z * d.x; // Half surface area
      if(cost < best_cost) {
        best_cost = cost;
        best_idx = i;
      }
    }
  }

  return best_idx;
}

// Walter et al: Fast Agglomerative Clustering for Rendering
uint32_t cluster_nodes(node *nodes, aabb *aabbs, uint32_t node_idx,
                       uint32_t *node_indices, uint32_t node_indices_cnt)
{
  uint32_t a = 0;
  uint32_t b = find_best_node(a, aabbs, node_indices, node_indices_cnt);
  while(node_indices_cnt > 1) {
    uint32_t c = find_best_node(b, aabbs, node_indices, node_indices_cnt);
    if(a == c) {
      uint32_t idx_a = node_indices[a];
      uint32_t idx_b = node_indices[b];

      node *node_a = &nodes[idx_a];
      node *node_b = &nodes[idx_b];

      // Claim new node which is the combination of node A and B
      node *new_node = &nodes[node_idx];

      // Either store the actual node idx or an idx to the data item
      // (Former leaf nodes of data items will simply be "forgotten")
      new_node->left = (node_a->left != node_a->right) ? idx_a : node_a->left;
      new_node->right = (node_b->left != node_b->right) ? idx_b : node_b->left;

      aabb *left_aabb = &aabbs[idx_a];
      new_node->lmin = left_aabb->min;
      new_node->lmax = left_aabb->max;

      aabb *right_aabb = &aabbs[idx_b];
      new_node->rmin = right_aabb->min;
      new_node->rmax = right_aabb->max;

      // Store combined aabb of left and right child of this node
      aabbs[node_idx] = aabb_combine(left_aabb, right_aabb);

      // Replace node A with newly created combined node
      node_indices[a] = node_idx--;

      // Remove node B by replacing its slot with last node
      node_indices[b] = node_indices[--node_indices_cnt];

      // Restart the loop for remaining nodes
      b = find_best_node(a, aabbs, node_indices, node_indices_cnt);
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

void blas_build(node *nodes, const tri *tris, uint32_t tri_cnt)
{
  uint32_t node_indices[tri_cnt];
  uint32_t node_indices_cnt = 0;

  // We will have 2 * tri_cnt - 1 nodes, but drop the leafs eventually
  // Nodes will be placed beginning at the back of the array
  uint32_t node_idx = 2 * tri_cnt - 2;
  aabb aabbs[2 * tri_cnt - 1];

  // Construct temporary leaf node for each tri
  for(uint32_t i = 0; i < tri_cnt; i++) {
    const tri *t = &tris[i];

    aabb *a = &aabbs[node_idx];
    *a = aabb_init();
    aabb_grow(a, t->v0);
    aabb_grow(a, t->v1);
    aabb_grow(a, t->v2);

    // Store temporary leaf node with negated data index
    node *n = &nodes[node_idx];
    n->left = n->right = ~i;

    node_indices[node_indices_cnt++] = node_idx--;
  }

  cluster_nodes(nodes, aabbs, node_idx, node_indices, node_indices_cnt);
}

void tlas_build(node *nodes, const inst_info *instances, uint32_t inst_cnt)
{
  uint32_t node_indices[inst_cnt];
  uint32_t node_indices_cnt = 0;

  // We will have 2 * inst_cnt - 1 nodes, but drop the leafs eventually
  // Nodes will be placed beginning at the back of the array
  uint32_t node_idx = 2 * inst_cnt - 2;
  aabb aabbs[2 * inst_cnt - 1];

  // Construct a leaf node for each instance
  for(uint32_t i = 0; i < inst_cnt; i++) {
    if((instances[i].state & IS_DISABLED) == 0) {
      aabb *a = &aabbs[node_idx];
      a->min = instances[i].box.min;
      a->max = instances[i].box.max;

      // Store temporary leaf node with negated data index
      node *n = &nodes[node_idx];
      n->left = n->right = ~i;

      node_indices[node_indices_cnt++] = node_idx--;
    }
  }

  node_idx =
      cluster_nodes(nodes, aabbs, node_idx, node_indices, node_indices_cnt);

  if(node_idx + 1 > 0)
    // There were gaps (not all assumed leaf nodes were populated), move root
    // node to front
    nodes[0] = nodes[node_idx + 1];
}
