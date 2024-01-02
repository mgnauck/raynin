#ifndef VIEW_H
#define VIEW_H

#include <stdint.h>
#include "vec3.h"

typedef struct cam cam;

typedef struct view {
  uint32_t  width;
  uint32_t  height;
  vec3      pix_delta_x;
  vec3      pix_delta_y;
  vec3      pix_top_left;
} view;

void view_update(view *v, cam *c);

#endif
