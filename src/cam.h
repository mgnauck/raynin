#ifndef CAM_H
#define CAM_H

#include "vec3.h"

typedef struct cam {
  vec3  eye;
  vec3  fwd;
  vec3  right;
  vec3  up;
  float theta;
  float phi;
  float vert_fov;
  float foc_dist;
  float foc_angle;
} cam;

void  cam_set(cam *c, vec3 look_from, vec3 look_at);
void  cam_set_s(cam *c, float theta, float phi);

#endif
