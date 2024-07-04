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
  int32_t   prnt;         // TODO Combine parent with l_idx (each 16 bit)
  int32_t   left;         // TODO Combine left/right into one (each 16 bit)
  int32_t   right;
  float     pad0;
  float     pad1;
} lnode;

// TODO Can we reference tris directly in the light tree avoiding dedicated ltris altogether?

void lighttree_build(lnode *nodes, ltri *ltris, uint32_t ltri_cnt);

#endif
