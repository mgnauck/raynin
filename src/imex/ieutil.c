#include "ieutil.h"

#include "../base/string.h"

const uint8_t *ie_read(void *dest, const uint8_t *src, size_t sz)
{
  memcpy(dest, src, sz);
  return src + sz;
}

uint8_t *ie_write(uint8_t *dest, const void *src, size_t sz)
{
  return (uint8_t *)memcpy(dest, src, sz) + sz;
}
