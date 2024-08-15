#include "cam.h"

void cam_calc_base(cam *c)
{
  c->right = vec3_cross((vec3){ 0.0f, 1.0f, 0.0f }, c->fwd);
  c->up = vec3_cross(c->fwd, c->right);
}

void cam_set(cam *c, vec3 look_from, vec3 look_at)
{
  c->eye = look_from;
  c->fwd = vec3_unit(vec3_sub(look_from, look_at));
  
  cam_calc_base(c);
}

void cam_set_dir(cam *c, vec3 dir)
{
  c->fwd = dir;

  cam_calc_base(c);
}
