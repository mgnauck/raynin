#ifndef EXPORT_H
#define EXPORT_H

#include <stddef.h>
#include <stdint.h>

typedef struct escene escene;

uint8_t export_bin(escene *scenes, uint8_t scene_cnt, uint8_t **bin, size_t *bin_sz);

#endif
