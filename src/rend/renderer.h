#ifndef RENDERER_H
#define RENDERER_H

#include <stdbool.h>
#include <stdint.h>

typedef struct scene scene;
typedef struct post_params post_params;

uint8_t renderer_gpu_alloc(uint32_t max_tri_cnt, uint32_t max_ltri_cnt,
                           uint32_t max_mtl_cnt, uint32_t max_inst_cnt);

void renderer_update(scene *s, const post_params *p, bool converge);

#endif
