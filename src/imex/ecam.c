#include "ecam.h"
#include "ieutil.h"

uint8_t ecam_calc_size(const ecam *c)
{
  return sizeof(c->eye) + sizeof(c->tgt) + sizeof(c->vert_fov);
}

uint8_t *ecam_write(uint8_t *dst, const ecam *c)
{
  dst = ie_write(dst, &c->eye, sizeof(c->eye));
  dst = ie_write(dst, &c->tgt, sizeof(c->tgt));
  dst = ie_write(dst, &c->vert_fov, sizeof(c->vert_fov));
  return dst;
}
