#ifndef SCN_H
#define SCN_H

#include <stddef.h>
#include <stdint.h>
#include "vec3.h"
#include "aabb.h"
#include "obj.h"

#define BUF_LINE_SIZE 4

typedef struct shape shape;
typedef struct mat mat;

typedef struct scn {
  uint32_t  *obj_buf;
  uint32_t  obj_idx;
  float     *shape_buf;
  uint32_t  shape_idx;
  float     *mat_buf;
  uint32_t  mat_idx;
} scn;

size_t    scn_shape_capacity(size_t sphere_cnt, size_t quad_cnt);

scn       *scn_init(size_t obj_buf_capacity, size_t shape_buf_capacity, size_t mat_buf_capacity);
void      scn_release(scn *s);

uint32_t  scn_add_obj(scn *s, obj o); 
uint32_t  scn_add_shape(scn *s, const void *shape, size_t size);
uint32_t  scn_add_mat(scn *s, const void *mat, size_t size);

obj       *scn_get_obj(const scn *s, uint32_t idx);
void      *scn_get_shape(const scn *s, uint32_t idx);
void      *scn_get_mat(const scn *s, uint32_t idx);

aabb      scn_get_obj_aabb(const scn *s, uint32_t idx);
vec3      scn_get_obj_center(const scn *s, uint32_t idx);

#endif
