#include "aabb.h"
#include <float.h>
#include "mutil.h"

aabb aabb_init()
{
  return (aabb){
    (vec3){{{ FLT_MAX, FLT_MAX, FLT_MAX }}},
    (vec3){{{ -FLT_MAX, -FLT_MAX, -FLT_MAX }}}
  };
}

aabb aabb_combine(aabb a, aabb b)
{
  return (aabb){ vec3_min(a.min, b.min), vec3_max(a.max, b.max) };
}

void aabb_grow(aabb *a, vec3 v)
{
  a->min = vec3_min(a->min, v);
  a->max = vec3_max(a->max, v);
}

void aabb_pad(aabb *a)
{
  for(uint8_t i=0; i<3; i++) {
    if(fabsf(a->max.v[i] - a->min.v[i]) < EPSILON) {
      a->min.v[i] -= EPSILON * 0.5;
      a->max.v[i] += EPSILON * 0.5;
    }
  }
}

float aabb_calc_area(aabb a)
{
  vec3 d = vec3_sub(a.max, a.min);
  return d.x * d.y + d.y * d.z + d.z * d.x;
}
