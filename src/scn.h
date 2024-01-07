#ifndef SCN_H
#define SCN_H

#include <stddef.h>
#include <stdint.h>
#include "vec3.h"
#include "aabb.h"

#define BUF_LINE_SIZE 4

typedef struct obj obj;
typedef struct shape shape;
typedef struct mat mat;

typedef struct scn {
  obj       *objs;
  uint32_t  obj_cnt;
  float     *shape_lines;
  uint32_t  shape_line_cnt;
  float     *mat_lines;
  uint32_t  mat_line_cnt;
} scn;

size_t    scn_calc_shape_lines(size_t sphere_cnt, size_t quad_cnt);

scn       *scn_init(size_t obj_cnt, size_t shape_line_cnt, size_t mat_line_cnt);
void      scn_release(scn *s);

uint32_t  scn_add_obj(scn *s, const obj *o); 
uint32_t  scn_add_shape(scn *s, const void *shape, size_t size);
uint32_t  scn_add_mat(scn *s, const void *mat, size_t size);

obj       *scn_get_obj(const scn *s, uint32_t idx);
void      *scn_get_shape(const scn *s, uint32_t idx);
void      *scn_get_mat(const scn *s, uint32_t idx);

aabb      scn_get_obj_aabb(const scn *s, uint32_t idx);
vec3      scn_get_obj_center(const scn *s, uint32_t idx);

#endif
