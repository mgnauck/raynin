#ifndef SCN_H
#define SCN_H

#include <stddef.h>
#include <stdint.h>
#include "vec3.h"
#include "aabb.h"

typedef struct obj obj;
typedef struct shape shape;
typedef struct mat mat;

typedef struct scn {
  obj       *objs;
  size_t    obj_cnt;
  float     *shape_buf;
  size_t    shape_buf_size;
  float     *mat_buf;
  size_t    mat_buf_size;
} scn;

size_t    scn_calc_shape_buf_size(size_t sphere_cnt, size_t quad_cnt);
size_t    scn_calc_mat_buf_size(size_t lambert_cnt, size_t metal_cnt,
            size_t glass_cnt, size_t emitter_cnt);

scn       *scn_init(size_t obj_cnt, size_t shape_buf_size, size_t mat_buf_size);
void      scn_release(scn *s);

void      scn_add_obj(scn *s, const obj *o); 
size_t    scn_add_shape(scn *s, const void *shape, size_t size);
size_t    scn_add_mat(scn *s, const void *mat, size_t size);

obj       *scn_get_obj(const scn *s, size_t idx);
void      *scn_get_shape(const scn *s, size_t ofs);
void      *scn_get_mat(const scn *s, size_t ofs);

aabb      scn_get_obj_aabb(const scn *s, size_t idx);
vec3      scn_get_obj_center(const scn *s, size_t idx);

#endif
