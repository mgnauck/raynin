#ifndef CAM_H
#define CAM_H

#include "../util/vec3.h"

typedef struct cam {
  vec3 eye;
  float vert_fov;
  vec3 right;
  float foc_dist;
  vec3 up;
  float foc_angle;
  vec3 fwd;
} cam;

void cam_set(cam *c, vec3 eye, vec3 fwd);
void cam_set_tgt(cam *c, vec3 look_from, vec3 look_at);
void cam_set_dir(cam *c, vec3 dir);

#endif
