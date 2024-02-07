#ifndef TLAS_H
#define TLAS_H

#include <stdint.h>
#include "vec3.h"

typedef struct inst inst;

typedef struct tlas_node {
  vec3      min;
  uint32_t  children; // 2x 16 bit
  vec3      max;
  uint32_t  inst;     // Leaf nodes only
} tlas_node;

void tlas_build(tlas_node *nodes, const inst *instances, uint32_t inst_cnt);

#endif
