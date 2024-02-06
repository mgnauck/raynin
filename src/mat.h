#ifndef MAT_H
#define MAT_H

#include "vec3.h"

typedef enum mat_type {
  LAMBERT = 0,
  ISOTROPIC,
  EMITTER,
  METAL,
  DIELECTRIC
} mat_type;

typedef struct mat {
  vec3  color;
  float value;
} mat;

void mat_rand(mat *m);

#endif
