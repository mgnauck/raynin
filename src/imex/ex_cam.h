#ifndef EX_CAM_H
#define EX_CAM_H

#include "../util/vec3.h"

typedef struct ex_cam {
  vec3  eye;
  vec3  tgt;
  float vert_fov;
} ex_cam;

#endif
