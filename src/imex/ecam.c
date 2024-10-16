#include "ecam.h"

#include "ieutil.h"

uint8_t ecam_calc_size(const ecam *c)
{
  return sizeof(c->pos) + sizeof(c->dir) + sizeof(c->vert_fov);
}

uint8_t *ecam_write(uint8_t *dst, const ecam *c)
{
  dst = ie_write(dst, &c->pos, sizeof(c->pos));
  dst = ie_write(dst, &c->dir, sizeof(c->dir));
  dst = ie_write(dst, &c->vert_fov, sizeof(c->vert_fov));
  return dst;
}
