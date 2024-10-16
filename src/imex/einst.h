#ifndef EINST_H
#define EINST_H

#include <stdint.h>

#include "../util/vec3.h"

typedef struct einst {
  uint32_t mesh_id; // node flags << 16 | mesh_id
  vec3 scale;
  float rot[4];
  vec3 trans;
} einst;

uint16_t einst_calc_size(const einst *inst);
uint8_t *einst_write(uint8_t *dst, const einst *inst);

#endif
