#ifndef MAT_H
#define MAT_H

#include "vec3.h"

typedef enum mat_type {
  LAMBERT,
  ISOTROPIC,
  EMITTER,
  METAL,
  DIELECTRIC
} mat_type;

// LAMBERT, ISOTROPIC, EMITTER
typedef struct basic {
  vec3  albedo;
  float pad;
} basic;

typedef struct metal {
  vec3  albedo;
  float fuzz_radius;
} metal;

typedef struct dielectric {
  vec3  albedo;
  float refr_idx;
} glass;

#endif
