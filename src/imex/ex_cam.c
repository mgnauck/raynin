#include "ex_cam.h"
#include "ieutil.h"

uint8_t ex_cam_calc_size(ex_cam const *c)
{
  return sizeof(c->eye) + sizeof(c->tgt) + sizeof(c->vert_fov);
}

uint8_t *ex_cam_write(ex_cam const *c, uint8_t *dst)
{
  dst = ie_write(dst, &c->eye, sizeof(c->eye));
  dst = ie_write(dst, &c->tgt, sizeof(c->tgt));
  dst = ie_write(dst, &c->vert_fov, sizeof(c->vert_fov));
  return dst;
}
