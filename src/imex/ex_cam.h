#ifndef EX_CAM_H
#define EX_CAM_H

#include "../util/vec3.h"

typedef struct ex_cam {
  vec3  eye;
  vec3  tgt;
  float vert_fov;
} ex_cam;

uint8_t ex_cam_calc_size(ex_cam const *c);

uint8_t *ex_cam_write(ex_cam const *c, uint8_t *dst);

#endif
