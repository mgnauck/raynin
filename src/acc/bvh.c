#include "bvh.h"
#include <float.h>
#include "../scene/inst.h"
#include "../scene/tri.h"
#include "../util/aabb.h"

static uint32_t find_best_node(node *nodes, uint32_t idx, uint32_t *node_indices, uint32_t node_indices_cnt)
{
  float     best_cost = FLT_MAX;
  uint32_t  best_idx;

  node *n = &nodes[node_indices[idx]];

  // Find smallest combined aabb of current node and any other node
  for(uint32_t i=0; i<node_indices_cnt; i++) {
    if(idx != i) {
      node *n2 = &nodes[node_indices[i]];
      vec3 d = vec3_sub(vec3_max(n->max, n2->max), vec3_min(n->min, n2->min));
      float cost = d.x * d.y + d.y * d.z + d.z * d.x; /// Half surface area
      if(cost < best_cost) {
        best_cost = cost;
        best_idx = i;
      }
    }
  }

  return best_idx;
}

// Walter et al: Fast Agglomerative Clustering for Rendering
void blas_build(node *nodes, const tri *tris, uint32_t tri_cnt)
{
  uint32_t node_indices[tri_cnt]; 
  uint32_t node_indices_cnt = 0;

  // Construct a leaf node for each tri
  for(uint32_t i=0; i<tri_cnt; i++) {
      const tri *t = &tris[i];

      aabb box = aabb_init();
      aabb_grow(&box, t->v0);
      aabb_grow(&box, t->v1);
      aabb_grow(&box, t->v2);

      // +1 due to reserved space for root node
      node *n = &nodes[1 + node_indices_cnt];
      n->min = box.min;
      n->max = box.max;
      n->children = 0;
      n->idx = i;
      
      node_indices[node_indices_cnt] = 1 + node_indices_cnt;
      node_indices_cnt++;
  }

  // Account for nodes so far
  uint32_t node_cnt = 1 + node_indices_cnt;

  // Bottom up combining of nodes
  uint32_t a = 0;
  uint32_t b = find_best_node(nodes, a, node_indices, node_indices_cnt);
  while(node_indices_cnt > 1) {
    uint32_t c = find_best_node(nodes, b, node_indices, node_indices_cnt);
    if(a == c) {
      uint32_t idx_a = node_indices[a];
      uint32_t idx_b = node_indices[b];

      node *node_a = &nodes[idx_a];
      node *node_b = &nodes[idx_b];

      // Claim new node which is the combination of node A and B
      node *new_node = &nodes[node_cnt];
      new_node->min = vec3_min(node_a->min, node_b->min);
      new_node->max = vec3_max(node_a->max, node_b->max);
      // Each child node index gets 16 bits
      new_node->children = (idx_b << 16) | idx_a;

      // Replace node A with newly created combined node
      node_indices[a] = node_cnt++;

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

  // Root node was formed last (at 2*n), move it to reserved index 0
  nodes[0] = nodes[--node_cnt];
}

void tlas_build(node *nodes, const inst_info *instances, uint32_t inst_cnt)
{
  uint32_t node_indices[inst_cnt]; 
  uint32_t node_indices_cnt = 0;

  // Construct a leaf node for each instance
  for(uint32_t i=0; i<inst_cnt; i++) {
    if(!(instances[i].state & IS_DISABLED)) { 
      node *n = &nodes[1 + node_indices_cnt]; // +1 due to reserved space for root node
      n->min = instances[i].box.min;
      n->max = instances[i].box.max;
      n->children = 0;
      n->idx = i;
      
      node_indices[node_indices_cnt] = 1 + node_indices_cnt;
      node_indices_cnt++;
    }
  }

  // Account for nodes so far
  uint32_t node_cnt = 1 + node_indices_cnt;

  // Bottom up combining of nodes
  uint32_t a = 0;
  uint32_t b = find_best_node(nodes, a, node_indices, node_indices_cnt);
  while(node_indices_cnt > 1) {
    uint32_t c = find_best_node(nodes, b, node_indices, node_indices_cnt);
    if(a == c) {
      uint32_t idx_a = node_indices[a];
      uint32_t idx_b = node_indices[b];

      node *node_a = &nodes[idx_a];
      node *node_b = &nodes[idx_b];

      // Claim new node which is the combination of node A and B
      node *new_node = &nodes[node_cnt];
      new_node->min = vec3_min(node_a->min, node_b->min);
      new_node->max = vec3_max(node_a->max, node_b->max);
      // Each child node index gets 16 bits
      new_node->children = (idx_b << 16) | idx_a;

      // Replace node A with newly created combined node
      node_indices[a] = node_cnt++;

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

  // Root node was formed last (at 2*n), move it to reserved index 0
  nodes[0] = nodes[--node_cnt];
}
