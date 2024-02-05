#include "tri.h"

void tri_calc_center(tri *t)
{
  t->center = vec3_scale(vec3_add(vec3_add(t->v0, t->v1), t->v2), 0.3333f);
}
