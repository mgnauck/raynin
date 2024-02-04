#ifndef TRI_H
#define TRI_H

#include "vec3.h"

typedef struct tri {
  vec3 v[3];
  vec3 center;
} tri;

typedef struct tri_data {
  vec3  n[3];
  float u[3];
  float v[3];
  float pad;
} tri_data;

void tri_calc_center(tri *t);

#endif
