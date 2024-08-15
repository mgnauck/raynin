#ifndef RENDERER_H
#define RENDERER_H

#include <stdbool.h>
#include <stdint.h>

typedef struct scene scene;

uint8_t       renderer_gpu_alloc(uint32_t max_tri_cnt, uint32_t max_ltri_cnt,
                                  uint32_t max_mtl_cnt, uint32_t max_inst_cnt);

void          renderer_update(scene *s, bool converge);

#endif
