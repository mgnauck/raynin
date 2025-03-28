#ifndef MAT_H
#define MAT_H

#include "vec3.h"

// LAMBERT, ISOTROPIC, EMITTER
typedef struct basic {
  vec3  albedo;
  float pad;
} basic;

typedef struct metal {
  vec3  albedo;
  float fuzz_radius;
} metal;

typedef struct glass {
  vec3  albedo;
  float refr_idx;
} glass;

#endif
