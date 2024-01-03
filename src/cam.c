#include "cam.h"
#include "mutil.h"
#include "sutil.h"

void cam_calc_base(cam *c)
{
  c->right = vec3_cross((vec3){{{ 0.0f, 1.0f, 0.0f }}}, c->fwd);
  c->up = vec3_cross(c->fwd, c->right);
}

void cam_set(cam *c, vec3 look_from, vec3 look_at)
{
  c->eye = look_from;
  c->fwd = vec3_unit(vec3_sub(look_from, look_at));

  cam_calc_base(c);

  c->theta = acosf(-c->fwd.y);
  c->phi = atan2f(-c->fwd.z, c->fwd.x) + PI;
}

void cam_set_dir(cam *c, float theta, float phi)
{
  c->theta = theta;
  c->phi = phi;

  c->fwd = vec3_spherical(theta, phi);
  
  cam_calc_base(c);
}

size_t cam_write(uint8_t *buf, const cam *c)
{
  uint8_t *p = buf;
  memcpy(p, &c->eye, sizeof(c->eye));
  p += sizeof(c->eye);
  memcpy(p, &c->vert_fov, sizeof(c->vert_fov));
  p += sizeof(c->vert_fov);
  memcpy(p, &c->right, sizeof(c->right));
  p += sizeof(c->right);
  memcpy(p, &c->foc_dist, sizeof(c->foc_dist));
  p += sizeof(c->foc_dist);
  memcpy(p, &c->up, sizeof(c->up));
  p += sizeof(c->up);
  memcpy(p, &c->foc_angle, sizeof(c->foc_angle));
  p += sizeof(c->foc_angle);
  return p - buf;
}
