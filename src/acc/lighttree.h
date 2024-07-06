#ifndef LIGHTTREE_H
#define LIGHTTREE_H

#include <stdint.h>
#include "../util/vec3.h"

typedef struct ltri ltri;

// Light node
typedef struct lnode {
  vec3      min;
  uint32_t  idx;          // 16 bit parent node id, 16 bit light id (= ltri id for now)
  vec3      max;
  uint32_t  children;     // 2x 16 bit child node id
  vec3      nrm;
  float     intensity;    // Sum of emission terms
} lnode;

void lighttree_build(lnode *nodes, ltri *ltris, uint32_t ltri_cnt);

#endif
