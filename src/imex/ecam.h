#ifndef ECAM_H
#define ECAM_H

#include "../util/vec3.h"

typedef struct ecam {
  vec3  eye;
  vec3  tgt;
  float vert_fov;
} ecam;

uint8_t ecam_calc_size(ecam const *c);
uint8_t *ecam_write(ecam const *c, uint8_t *dst);

#endif
