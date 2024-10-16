#ifndef ECAM_H
#define ECAM_H

#include "../util/vec3.h"

typedef struct ecam {
  vec3 pos;
  vec3 dir;
  float vert_fov;
} ecam;

uint8_t ecam_calc_size(const ecam *c);
uint8_t *ecam_write(uint8_t *dst, const ecam *c);

#endif
