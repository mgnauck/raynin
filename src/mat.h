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

lambert   lambert_create(vec3 albedo);
metal     metal_create(vec3 albedo, float fuzz_radius);
glass     glass_create(vec3 albedo, float refr_idx);

#endif
