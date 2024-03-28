#ifndef TLAS_H
#define TLAS_H

#include <stdint.h>
#include "vec3.h"

#define TLAS_NODE_MASK 0xffff

typedef struct inst inst;
typedef struct inst_info inst_info;

typedef struct tlas_node {
  vec3      min;
  uint32_t  children; // 2x 16 bit child node id
  vec3      max;
  uint32_t  inst;     // Inst id at leaf nodes only
} tlas_node;

void tlas_build(tlas_node *nodes, const inst *instances, const inst_info *inst_info, uint32_t inst_cnt);

#endif
