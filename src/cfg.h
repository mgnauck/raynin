#ifndef CFG_H
#define CFG_H

#include <stddef.h>
#include <stdint.h>

typedef struct cfg {
  uint32_t  width;
  uint32_t  height;
  uint32_t  spp;
  uint32_t  bounces;
} cfg;

size_t cfg_write(uint8_t *buf, cfg *cfg);

#endif
