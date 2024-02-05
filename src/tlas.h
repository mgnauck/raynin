#ifndef TLAS_H
#define TLAS_H

#include <stddef.h>
#include "vec3.h"

typedef struct inst inst;

typedef struct tlas_node {
  vec3      min;
  size_t    children; // 2x 16 bit
  vec3      max;
  size_t    inst;     // Leaf nodes only
} tlas_node;

void tlas_build(tlas_node *nodes, const inst *instances, size_t inst_cnt);

#endif
