#ifndef TRI_H
#define TRI_H

#include "vec3.h"

typedef struct tri {
  vec3 v0;
  float pad0;
  vec3 v1;
  float pad1;
  vec3 v2;
  float pad2;
  vec3 center;
  float pad3;
} tri;

typedef struct tri_data {
  vec3  n0;
  float pad0;
  vec3  n1;
  float pad1;
  vec3  n2;
  float pad2;
} tri_data;

void tri_calc_center(tri *t);

#endif
