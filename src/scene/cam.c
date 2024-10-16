#include "cam.h"

void cam_calc_base(cam *c)
{
  c->right = vec3_unit(vec3_cross((vec3){0.0f, 1.0f, 0.0f}, c->fwd));
  c->up = vec3_unit(vec3_cross(c->fwd, c->right));
}

void cam_set(cam *c, vec3 eye, vec3 fwd)
{
  c->eye = eye;
  c->fwd = vec3_unit(fwd);

  cam_calc_base(c);
}

void cam_set_tgt(cam *c, vec3 look_from, vec3 look_at)
{
  cam_set(c, look_from, vec3_sub(look_from, look_at));
}

void cam_set_dir(cam *c, vec3 dir)
{
  c->fwd = vec3_unit(dir);

  cam_calc_base(c);
}
