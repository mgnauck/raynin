#ifndef LIGHTTREE_H
#define LIGHTTREE_H

#include <stdint.h>
#include "../util/vec3.h"

typedef struct ltri ltri;

// Light node
typedef struct lnode {
  vec3      min;
  int32_t   l_idx;        // Light idx, but is only ltri idx for now
  vec3      max;
  float     intensity;    // Sum of emission terms
  vec3      nrm;
  int32_t   prnt;         // TODO Combine parent/left/right so we need less space? Would mean max 2048 nodes :(
  int32_t   left;
  int32_t   right;
  float     pad0;
  float     pad1;
} lnode;

void lighttree_build(lnode *nodesi, ltri *ltris, uint32_t ltri_cnt);

#endif
