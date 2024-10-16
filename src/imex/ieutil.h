#ifndef IE_UTIL_H
#define IE_UTIL_H

#include <stddef.h>
#include <stdint.h>

const uint8_t *ie_read(void *dest, const uint8_t *src, size_t sz);
uint8_t *ie_write(uint8_t *dest, const void *src, size_t sz);

#endif
