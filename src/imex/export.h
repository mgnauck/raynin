#ifndef EXPORT_H
#define EXPORT_H

#include <stddef.h>
#include <stdint.h>

// #define EXPORT_CAMS
// #define EXPORT_NORMALS
// #define EXPORT_TRIS

typedef struct escene escene;

uint8_t export_bin(uint8_t **bin, size_t *bin_sz, escene *scenes,
                   uint8_t scene_cnt);

#endif
