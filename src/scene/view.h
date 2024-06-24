#ifndef VIEW_H
#define VIEW_H

#include <stdint.h>
#include "../util/vec3.h"

typedef struct cam cam;

typedef struct view {
  vec3      pix_delta_x;
  float     pad0;
  vec3      pix_delta_y;
  float     pad1;
  vec3      pix_top_left;
  float     pad2;
} view;

void view_calc(view *v, uint32_t width, uint32_t height, cam *c);

#endif
