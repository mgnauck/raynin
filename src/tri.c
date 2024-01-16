#include "tri.h"

vec3 tri_calc_center(const tri *t)
{
  return vec3_scale(vec3_add(vec3_add(t->v[0], t->v[1]), t->v[2]), 0.3333f);
}
