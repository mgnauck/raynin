#ifndef MAT_H
#define MAT_H

#include "vec3.h"

typedef struct mat {
  vec3  color;
  float value;
} mat;

void mat_rand(mat *m);

#endif
