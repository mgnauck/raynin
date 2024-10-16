#include "aabb.h"

#include <float.h>

#include "../base/math.h"

aabb aabb_init()
{
  return (aabb){(vec3){FLT_MAX, FLT_MAX, FLT_MAX},
                (vec3){-FLT_MAX, -FLT_MAX, -FLT_MAX}};
}

aabb aabb_combine(const aabb *a, const aabb *b)
{
  return (aabb){vec3_min(a->min, b->min), vec3_max(a->max, b->max)};
}

void aabb_grow(aabb *a, vec3 v)
{
  a->min = vec3_min(a->min, v);
  a->max = vec3_max(a->max, v);
}

void aabb_pad(aabb *a)
{
  for(uint8_t i = 0; i < 3; i++) {
    float mi = vec3_get(a->min, i);
    float ma = vec3_get(a->max, i);
    if(fabsf(ma - mi) < EPSILON) {
      vec3_set(&a->min, i, mi - EPSILON * 0.5);
      vec3_set(&a->max, i, ma + EPSILON * 0.5);
    }
  }
}

float aabb_calc_area(const aabb *a)
{
  vec3 d = vec3_sub(a->max, a->min);
  return d.x * d.y + d.y * d.z + d.z * d.x;
}
