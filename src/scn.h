#ifndef SCN_H
#define SCN_H

#include <stddef.h>
#include <stdint.h>
#include "vec3.h"
#include "obj.h"
#include "shape.h"
#include "mat.h"

#define BUF_LINE_SIZE 4

typedef struct scn {
  uint32_t  *obj_buf;
  uint32_t  obj_idx;
  float     *shape_buf;
  uint32_t  shape_idx;
  float     *mat_buf;
  uint32_t  mat_idx;
} scn;

scn       *scn_init(size_t obj_buf_capacity, size_t shape_buf_capacity, size_t mat_buf_capacity);
void      scn_release(scn *s);

uint32_t  scn_add_obj(scn *s, obj o); 

uint32_t  scn_add_sphere(scn *s, sphere shape);
uint32_t  scn_add_quad(scn *s, quad shape);

uint32_t  scn_add_lambert(scn *s, lambert mat);
uint32_t  scn_add_metal(scn *s, metal mat);
uint32_t  scn_add_glass(scn *s, glass mat);
uint32_t  scn_add_emitter(scn *s, emitter mat);

obj       *scn_get_obj(const scn *s, uint32_t idx);

aabb      scn_get_obj_aabb(const scn *s, uint32_t idx);
vec3      scn_get_obj_center(const scn *s, uint32_t idx);

sphere    *scn_get_sphere(const scn *s, uint32_t idx);
quad      *scn_get_quad(const scn *s, uint32_t idx);

lambert   *scn_get_lambert(const scn *s, uint32_t idx);
metal     *scn_get_metal(const scn *s, uint32_t idx);
glass     *scn_get_glass(const scn *s, uint32_t idx);
emitter   *scn_get_emitter(const scn *s, uint32_t idx);

#endif
