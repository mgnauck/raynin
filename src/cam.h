#ifndef CAM_H
#define CAM_H

#include "vec3.h"

#define CAM_BUF_SIZE 12 * 4

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

#endif
