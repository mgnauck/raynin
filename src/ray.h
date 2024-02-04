#ifndef RAY_H
#define RAY_H

#include "vec3.h"
#include "mat4.h"

typedef struct view view;
typedef struct cam cam;

typedef struct ray {
  vec3  ori;
  vec3  dir;
  vec3  inv_dir;
} ray;

void ray_create(ray *ray, vec3 ori, vec3 dir);
void ray_create_primary(ray *ray, float x, float y, const view *v, const cam *c);
void ray_transform(ray *dest, const mat4 m, const ray *r);

#endif
