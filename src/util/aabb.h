#ifndef AABB_H
#define AABB_H

#include "vec3.h"

typedef struct aabb {
  vec3 min;
  vec3 max;
} aabb;

aabb aabb_init();
aabb aabb_combine(const aabb *a, const aabb *b);
void aabb_grow(aabb *a, vec3 v);
void aabb_pad(aabb *a);
float aabb_calc_area(const aabb *a);

#endif
