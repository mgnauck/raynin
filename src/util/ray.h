#ifndef RAY_H
#define RAY_H

#include "../util/mat4.h"
#include "../util/vec3.h"

typedef struct ray {
  vec3  ori;
  vec3  dir;
  vec3  inv_dir;
} ray;

void ray_create(ray *ray, vec3 ori, vec3 dir);
void ray_transform(ray *dest, const mat4 m, const ray *r);

#endif
