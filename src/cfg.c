#include "cfg.h"
#include "sutil.h"

size_t cfg_write(uint8_t *buf, cfg *c)
{
  memcpy(buf, c, sizeof(*c));
  return sizeof(*c);
}
