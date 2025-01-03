#include "bvh.h"
#include <float.h>
#include "../base/stdlib.h"
#include "../base/string.h"
#include "../scene/inst.h"
#include "../scene/tri.h"
#include "../util/aabb.h"

// Fixed len node scratch buffer utilized on when rearranging nodes in memory
#define MAX_TEMP_NODES 16384
static bvhnode tnodes[MAX_TEMP_NODES];

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

/*void print_nodes(bvhnode *nodes, uint32_t node_cnt)
{
  for(uint32_t i=0; i<node_cnt; i++) {
    bvhnode *n = &nodes[i];
    logc("* Node %u", i);
    logc("  min: %6.3f, %6.3f, %6.3f, max: %6.3f, %6.3f, %6.3f",
        n->min.x, n->min.y, n->min.z, n->max.x, n->max.y, n->max.z);
    logc("  lnk: %u << 16 | %u", n->children >> 16, n->children & 0xffff);
    logc("  idx: %u", n->idx);
  }
}*/

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
      // Indicate interior node (consumed by hit/miss link traversal)
      new_node->idx = 0xffffffff;

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

// Reorder given nodes to depth-first in memory
void reorder_nodes(bvhnode *dnodes, bvhnode *snodes)
{
  // Stacks for source and dst nodes
  bvhnode   *src[64];
  bvhnode   *dst[64]; // Stores dst nodes that still need adjustment
  uint32_t  spos = 0;

  // Current index of dst node array
  uint32_t  idx = 0;

  bvhnode   *sn = snodes;
  
  while(sn) {
    bvhnode *dn = &dnodes[idx++];
    memcpy(dn, sn, sizeof(*dn));
    
    if(sn->children > 0) {
      // Assign next dst node array index as left child index
      dn->children = idx;
      // Right child of source nodes goes on source node stack
      src[spos] = &snodes[sn->children >> 16];
      // Current dst node stored on target node stack, so we can update
      // its right child index later on
      dst[spos++] = dn;
      // New source node is left child
      sn = &snodes[sn->children & 0xffff];
    } else {
      if(spos > 0) {
        // Pop new source node from stack
        sn = src[--spos];
        // Adjust right child index of dst node on stack
        dst[spos]->children |= idx << 16;
      } else
        break;
    }
  }
}

// Reconnect nodes via hit/miss links
void reconnect_nodes(bvhnode *dnodes, bvhnode *snodes, uint8_t axis, uint8_t sign)
{
  uint32_t  stack[64];
  uint32_t  spos = 0;

  bvhnode   *sn = snodes;
  bvhnode   *dn = dnodes;

  while(sn) {
    memcpy(dn, sn, sizeof(*dn));
    if(dn->children == 0) {
      // Leaf node
      if(spos == 0) {
        // No node on stack, assign hit/miss terminal (last node overall)
        dn->children = 0xffffffff;
        break;
      } else {
        // Pop node of stack
        uint32_t idx = stack[--spos];
        // Assign index of popped node as hit and miss link
        dn->children = (idx << 16) | idx;
        // Continue with popped node
        sn = &snodes[idx];
        dn = &dnodes[idx];
      }
    } else {
      // Interior node
      uint32_t children = dn->children;
      uint32_t left_child = children & 0xffff;
      // If no node is on stack (= current is right child), assign terminal as
      // miss link. Else, assign node on stack (right sibling) as miss link.
      dn->children =
        ((spos == 0) ? (0xffff << 16) : (stack[spos - 1] << 16))
        // Hit link is always the next node (left child) and already assigned
        | left_child;
      // Next node is left child
      sn = &snodes[left_child];
      dn = &dnodes[left_child];
      // Put right child on stack
      stack[spos++] = children >> 16;
    }
  }
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
    aabb_pad(&a);

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

  reorder_nodes(tnodes, nodes);

  // Prepare threaded bvh for each axis in neg/pos direction, i.e. +X, -X, ..
  for(uint8_t i=0; i<6; i++)
    reconnect_nodes(&nodes[2 * tri_cnt * i], tnodes, i / 2, i % 2);
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
    if((instances[i].state & IS_DISABLED) == 0) { // Need to handle gaps
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

  reorder_nodes(tnodes, nodes);

  for(uint8_t i=0; i<6; i++)
    reconnect_nodes(&nodes[2 * inst_cnt * i], tnodes, i / 2, i % 2);
}
