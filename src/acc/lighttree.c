#include "lighttree.h"
#include <float.h>
#include "../scene/tri.h"
#include "../util/aabb.h"

static uint32_t find_best_node(lnode *nodes, uint32_t idx, uint32_t *node_indices, uint32_t node_indices_cnt)
{
  float best_cost = FLT_MAX;
  uint32_t best_idx;

  lnode *curr = &nodes[node_indices[idx]];

  // Find smallest cost of combining current node and any other node
  for(uint32_t i=0; i<node_indices_cnt; i++) {
    if(idx != i) {
      lnode *other = &nodes[node_indices[i]];
      vec3 diag = vec3_sub(vec3_max(curr->max, other->max), vec3_min(curr->min, other->min));
      float cost = (curr->intensity + other->intensity) * vec3_dot(diag, diag);
      if(cost < best_cost) {
        best_cost = cost;
        best_idx = i;
      }
    }
  }

  return best_idx;
}

// Walter et al: Fast Agglomerative Clustering for Rendering
// Walter et al: Lightcuts: A Scalable Approach to Illumination
// Cem Yuksel: Stochastic Lightcuts for Sampling Many Lights
void lighttree_build(lnode *nodes, ltri *ltris, uint32_t ltri_cnt)
{
  uint32_t node_indices[ltri_cnt];
  uint32_t node_indices_cnt = 0;
  uint32_t ofs = 1; // Reserve space for root node

  // Construct a leaf node for each light
  for(uint32_t i=0; i<ltri_cnt; i++) {
    lnode *n = &nodes[ofs + node_indices_cnt];

    n->l_idx = i; // lnode i + 1 references light i

    n->left = n->right = n->prnt = -1;

    ltri *lt = &ltris[i];

    n->intensity = lt->emission.x + lt->emission.y + lt->emission.z;
    n->nrm = lt->nrm;

    aabb box;
    aabb_grow(&box, lt->v0);
    aabb_grow(&box, lt->v1);
    aabb_grow(&box, lt->v2);

    n->min = box.min;
    n->max = box.max;

    node_indices[node_indices_cnt] = ofs + node_indices_cnt;
    node_indices_cnt++;
  }

  // Account for nodes so far
  uint32_t node_cnt = ofs + node_indices_cnt;

  // Bottom up combining of light nodes
  uint32_t a = 0;
  uint32_t b = find_best_node(nodes, a, node_indices, node_indices_cnt);
  while(node_indices_cnt > 1) {
    uint32_t c = find_best_node(nodes, b, node_indices, node_indices_cnt);
    if(a == c) {
      uint32_t idx_a = node_indices[a];
      uint32_t idx_b = node_indices[b];

      lnode *node_a = &nodes[idx_a];
      lnode *node_b = &nodes[idx_b];

      // Claim new node which is the combination of node A and B
      lnode *new_node = &nodes[node_cnt];

      new_node->min = vec3_min(node_a->min, node_b->min);
      new_node->max = vec3_max(node_a->max, node_b->max);

      new_node->intensity = node_a->intensity + node_b->intensity;

      new_node->left  = idx_a;
      new_node->right = idx_b;

      if(vec3_dot(node_a->nrm, node_b->nrm) > 0.9f)
        // Keep the "same" normal of the children
        new_node->nrm = vec3_unit(vec3_add(node_a->nrm, node_b->nrm));
      else
        new_node->nrm = (vec3){ 0.0f, 0.0f, 0.0f };

      // Set new node as parent of child nodes (consider final move of root node!)
      node_a->prnt = node_b->prnt = node_indices_cnt >= 2 ? node_cnt : 0;

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
