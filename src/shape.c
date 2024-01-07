#include "shape.h"

aabb sphere_get_aabb(const sphere *s)
{
  float r = s->radius;
  return (aabb){
    vec3_sub(s->center, (vec3){{{ r, r, r }}}),
    vec3_add(s->center, (vec3){{{ r, r, r }}}) };
}

aabb quad_get_aabb(const quad *q)
{
  aabb a = aabb_init();
  aabb_grow(&a, q->q);
  aabb_grow(&a, vec3_add(vec3_add(q->q, q->u), q->v));
  aabb_pad(&a);
  return a;
}

vec3 quad_get_center(const quad *q)
{
  return vec3_add(
      vec3_add(q->q, vec3_scale(q->u, 0.5f)), vec3_scale(q->v, 0.5f));
}
