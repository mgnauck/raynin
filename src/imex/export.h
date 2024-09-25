#ifndef EXPORT_H
#define EXPORT_H

#include <stddef.h>
#include <stdint.h>

typedef struct ex_scene ex_scene;

uint8_t export_bin(ex_scene *scenes, uint8_t scene_cnt, uint8_t **bin, size_t *bin_sz);

#endif
