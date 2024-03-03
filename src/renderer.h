#ifndef RENDERER_H
#define RENDERER_H

#include <stdint.h>
#include "types.h"
#include "vec3.h"

typedef struct scene scene;
typedef struct render_data render_data;

render_data   *renderer_init(scene *s, uint16_t width, uint16_t height, uint8_t spp);
void          renderer_release(render_data *rd);
void          renderer_set_dirty(render_data *rd, res_type r);
void          renderer_set_bg_col(render_data *rd, vec3 bg_col);
void          renderer_push_static_res(render_data *rd);
void          renderer_update(render_data *rd, float time);

#endif
