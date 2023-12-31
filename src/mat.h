#ifndef MAT_H
#define MAT_H

#include "vec3.h"

typedef struct lambert {
  vec3  albedo;
  float pad0;
} lambert;

typedef struct metal {
  vec3  albedo;
  float fuzz_radius;
} metal;

typedef struct glass {
  vec3  albedo;
  float refr_idx;
} glass;

lambert   mat_create_lambert(vec3 albedo);
metal     mat_create_metal(vec3 albedo, float fuzz_radius);
glass     mat_create_glass(vec3 albedo, float refr_idx);

#endif
