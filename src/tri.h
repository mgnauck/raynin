#ifndef TRI_H
#define TRI_H

#include "vec3.h"

typedef struct tri {
  vec3 v[3];
} tri;

vec3 tri_calc_center(const tri *t);

#endif
