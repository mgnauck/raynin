#ifndef EINST_H
#define EINST_H

#include <stdint.h>
#include "../util/vec3.h"

typedef struct einst {
  uint16_t  mesh_id;
  vec3      scale;
  float     rot[4];
  vec3      trans;
} einst;

uint16_t einst_calc_size(einst const *inst);
uint8_t *einst_write(uint8_t *dst, einst const *inst);

#endif
