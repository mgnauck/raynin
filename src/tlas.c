#include "tlas.h"
#include <float.h>
#include "aabb.h"
#include "inst.h"

uint32_t find_best_node(tlas_node *nodes, uint32_t idx, uint32_t *node_indices,
    uint32_t node_indices_cnt)
{
  float best_area = FLT_MAX;
  uint32_t best_idx;

  uint32_t curr_idx = node_indices[idx];
  vec3 curr_min = nodes[curr_idx].min;
  vec3 curr_max = nodes[curr_idx].max;

  // Find smallest combined aabb of current node and any other node
  for(uint32_t i=0; i<node_indices_cnt; i++) {
    if(idx != i) {
      uint32_t other_idx = node_indices[i];
      vec3 mi = vec3_min(curr_min, nodes[other_idx].min);
      vec3 ma = vec3_max(curr_max, nodes[other_idx].max);
      float area = aabb_calc_area((aabb){ mi, ma });
      if(area < best_area) {
        best_area = area;
        best_idx = i;
      }
    }
  }

  return best_idx;
}

// Walter et al: Fast Agglomerative Clustering for Rendering
void tlas_build(tlas_node *nodes, const inst *instances, uint32_t inst_cnt)
{
  uint32_t node_indices_cnt = inst_cnt;
  uint32_t node_indices[inst_cnt];

  // Reserve space for root node + skipped 1st node
  uint32_t ofs = 2;

  // Construct leaf node for each instance
  for(uint32_t i=0; i<inst_cnt; i++) {
    tlas_node *n = &nodes[ofs + i];
    n->min = instances[i].min;
    n->max = instances[i].max;
    n->children = 0;
    n->inst = i;
    node_indices[i] = ofs + i;
  }

  // Account for nodes so far
  uint32_t node_cnt = ofs + inst_cnt;

  // Bottom up combining of tlas nodes
  uint32_t a = 0;
  uint32_t b = find_best_node(nodes, a, node_indices, node_indices_cnt);
  while(node_indices_cnt > 1) {
    uint32_t c = find_best_node(nodes, b, node_indices, node_indices_cnt);
    if(a == c) {
      uint32_t idx_a = node_indices[a];
      uint32_t idx_b = node_indices[b];

      tlas_node *node_a = &nodes[idx_a];
      tlas_node *node_b = &nodes[idx_b];

      // Claim new node which is the combination of node A and B
      tlas_node *new_node = &nodes[node_cnt];
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

  // Root node was formed last (at 2*n+1), move it to reserved index 0
  nodes[0] = nodes[--node_cnt];
}
