#include "bvh.h"
#include <float.h>
#include "../scene/inst.h"
#include "../scene/tri.h"
#include "../sys/sutil.h"
#include "../sys/log.h"
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

void reorder_nodes(node *nodes, uint32_t node_cnt)
{
#define MAX_NODE_CNT 4096
#define STACK_SIZE 32

  if(node_cnt >= MAX_NODE_CNT) { // Debug
    logc("Failed to reorder nodes because temporary node array is too small.");
    return; // Error
  }

  node      tnodes[MAX_NODE_CNT];

  node      *stack[STACK_SIZE];
  uint32_t  sidx = 0;

  node      *n  = nodes;  // Start at root
  uint32_t  cnt = 1;      // Root is already added

  // Copy all nodes to temporary nodes buffer
  memcpy(tnodes, nodes, node_cnt * sizeof(*nodes));

  while(1) {
    if(n->children > 0) {
      // Array pos of left child node
      // Right child is implicitly + 1
      node *ln = &nodes[cnt];

      // Copy left and right child of current node to output
      memcpy(ln, &tnodes[n->children & 0xffff], sizeof(*ln));
      memcpy(ln + 1, &tnodes[n->children >> 16], sizeof(*ln));
      
      // Assign adjusted child indices to current node
      n->children = ((cnt + 1) << 16) | cnt;

      // Left child will be processed next
      n = ln;

      // Right child will be processed later
      stack[sidx++] = ln + 1;

      // Account for two nodes added
      cnt += 2;
    } else {
      // Leaf node
      // Pop next node from stack if available
      if(sidx > 0)
        n = stack[--sidx];
      else
        return;
    }
  }
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

  // Reorder nodes to depth first
  //reorder_nodes(nodes, node_cnt);
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

  // Reorder nodes to depth first
  //reorder_nodes(nodes, node_cnt);
}
