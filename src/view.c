#include "view.h"
#include "cam.h"
#include "mutil.h"

void view_calc(view *v, uint32_t width, uint32_t height, cam *c)
{
  float v_height = 2.0f * tanf(0.5f * c->vert_fov * PI / 180.0f) * c->foc_dist;
  float v_width = v_height * (float)width / (float)height;

  vec3 v_right = vec3_scale(c->right, v_width);
  vec3 v_down = vec3_scale(c->up, -v_height);

  v->pix_delta_x = vec3_scale(v_right, 1.0f / width);
  v->pix_delta_y = vec3_scale(v_down, 1.0f / height);

  // viewport_top_left = eye - foc_dist * fwd - 0.5 * (viewport_right + viewport_down);
  vec3 v_top_left = vec3_add(c->eye,
      vec3_add(
        vec3_scale(c->fwd, -c->foc_dist),
        vec3_scale(vec3_add(v_right, v_down), -0.5f)));

  // pixel_top_left = viewport_top_left + 0.5 * (pixel_delta_x + pixel_delta_y)
  v->pix_top_left = vec3_add(v_top_left, 
      vec3_scale(vec3_add(v->pix_delta_x, v->pix_delta_y), 0.5f));
}
