#include "shape.h"

sphere sphere_create(vec3 center, float radius)
{
  return (sphere){ .center = center, .radius = radius };
}

quad quad_create(vec3 q, vec3 u, vec3 v)
{
  return (quad){ .q = q, .u = u, .v = v };
}

aabb sphere_aabb(const sphere *s)
{
  float r = s->radius;
  return (aabb){
    vec3_sub(s->center, (vec3){{{ r, r, r }}}),
    vec3_add(s->center, (vec3){{{ r, r, r }}}) };
}

aabb quad_aabb(const quad *q)
{
  aabb a = aabb_init();
  aabb_grow(&a, q->q);
  aabb_grow(&a, vec3_add(vec3_add(q->q, q->u), q->v));
  aabb_pad(&a);
  return a;
}
