#ifndef RENDERER_H
#define RENDERER_H

#include <stdbool.h>
#include <stdint.h>
#include "../settings.h"
#include "../util/vec3.h"

typedef struct scene scene;
typedef struct render_data render_data;

#ifdef NATIVE_BUILD
typedef struct SDL_Surface SDL_Surface;
#endif

uint8_t       renderer_gpu_alloc(uint32_t max_tri_cnt, uint32_t max_ltri_cnt,
                                  uint32_t max_mtl_cnt, uint32_t max_inst_cnt);

render_data   *renderer_init(scene *s, uint16_t width, uint16_t height);
void          renderer_release(render_data *rd);

void          renderer_setup(render_data *rd, uint32_t spp);
void          renderer_update(render_data *rd, uint32_t spp, bool converge);

#ifdef NATIVE_BUILD
void          renderer_render(render_data *rd, SDL_Surface *surface);
#endif

#endif
