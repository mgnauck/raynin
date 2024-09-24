#ifndef EX_INST_H
#define EX_INST_H

#include <stdint.h>
#include "../util/vec3.h"

typedef struct ex_inst {
  uint16_t  mesh_id;
  vec3      scale;
  float     rot[4];
  vec3      trans;
} ex_inst;

#endif
