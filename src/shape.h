#ifndef SHAPE_H
#define SHAPE_H

#include "vec3.h"
#include "aabb.h"

typedef struct sphere {
  vec3  center;
  float radius;
} sphere;

typedef struct quad {
  vec3  q;
  float pad0;
  vec3  u;
  float pad1;
  vec3  v;
  float pad2;
} quad;

sphere    sphere_create(vec3 center, float radius);
quad      quad_create(vec3 q, vec3 u, vec3 v);

aabb      sphere_aabb(const sphere *s);
aabb      quad_aabb(const quad *q);

#endif
