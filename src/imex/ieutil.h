#ifndef IE_UTIL_H
#define IE_UTIL_H

#include <stddef.h>
#include <stdint.h>

uint8_t *ie_read(void *dest, uint8_t *src, size_t sz);
uint8_t *ie_write(uint8_t *dest, void const *src, size_t sz);

#endif
