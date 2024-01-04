#ifndef CFG_H
#define CFG_H

#include <stdint.h>

#define TEMPORAL_WEIGHT 0.1f

typedef struct cfg {
  uint32_t  width;
  uint32_t  height;
  uint32_t  spp;
  uint32_t  bounces;
} cfg;

#endif
