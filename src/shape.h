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

aabb      sphere_get_aabb(const sphere *s);
aabb      quad_get_aabb(const quad *q);
vec3      quad_get_center(const quad *q);

#endif
