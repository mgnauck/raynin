#ifndef SHAPE_H
#define SHAPE_H

#include "vec3.h"

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

sphere    shape_create_sphere(vec3 center, float radius);
quad      shape_create_quad(vec3 q, vec3 u, vec3 v);

#endif
