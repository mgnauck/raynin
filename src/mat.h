#ifndef MAT_H
#define MAT_H

#include "vec3.h"

typedef struct lambert {
  vec3  albedo;
  float pad;
} lambert;

typedef struct metal {
  vec3  albedo;
  float fuzz_radius;
} metal;

typedef struct glass {
  vec3  albedo;
  float refr_idx;
} glass;

typedef struct emitter {
  vec3  albedo;
  float pad;
} emitter;

#endif
