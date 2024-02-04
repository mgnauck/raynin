#include "tri.h"

void tri_calc_center(tri *t)
{
  t->center = vec3_scale(vec3_add(vec3_add(t->v[0], t->v[1]), t->v[2]), 0.3333f);
}
