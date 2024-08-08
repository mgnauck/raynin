#ifndef CAM_H
#define CAM_H

#include "../util/vec3.h"

#define CAM_SIZE 12 * 4

typedef struct ray ray;
typedef struct view view;

typedef struct cam {
  // GPU data
  vec3  eye;
  float vert_fov;
  vec3  right;
  float foc_dist;
  vec3  up;
  float foc_angle;
  // Additional data
  vec3  fwd;
} cam;

void cam_set(cam *c, vec3 look_from, vec3 look_at);
void cam_set_dir(cam *c, vec3 dir);

void cam_create_primary_ray(const cam *c, ray *ray, float x, float y, const view *v);

#endif
