#include "scn.h"
#include "sutil.h"
#include "obj.h"
#include "shape.h"
#include "mat.h"

size_t scn_calc_shape_lines(size_t sphere_cnt, size_t quad_cnt)
{
  return sphere_cnt * sizeof(sphere) / (BUF_LINE_SIZE * sizeof(float)) +
    quad_cnt * sizeof(quad) / (BUF_LINE_SIZE * sizeof(float));
}

scn *scn_init(size_t obj_cnt, size_t shape_line_cnt, size_t mat_line_cnt)
{
  scn *s = malloc(sizeof(*s));

  s->objs = malloc(obj_cnt * sizeof(*s->objs));
  s->obj_cnt = 0;
  
  s->shape_lines = malloc(shape_line_cnt * BUF_LINE_SIZE * sizeof(*s->shape_lines));
  s->shape_line_cnt = 0;
  
  s->mat_lines = malloc(mat_line_cnt * BUF_LINE_SIZE * sizeof(*s->mat_lines));
  s->mat_line_cnt = 0;
  
  return s;
}

void scn_release(scn *s)
{
  free(s->objs);
  free(s->shape_lines);
  free(s->mat_lines);
  free(s);
}

uint32_t scn_add_obj(scn *s, const obj *obj)
{
  memcpy(&s->objs[s->obj_cnt], obj, sizeof(*obj));
  return s->obj_cnt++;
}

uint32_t scn_add_shape(scn *s, const void *shape, size_t size)
{
  memcpy(&s->shape_lines[s->shape_line_cnt * BUF_LINE_SIZE], shape, size);
  
  size_t cnt = s->shape_line_cnt;
  s->shape_line_cnt += size / (BUF_LINE_SIZE * sizeof(*s->shape_lines));

  return cnt;
}

uint32_t scn_add_mat(scn *s, const void *mat, size_t size)
{
  memcpy(&s->mat_lines[s->mat_line_cnt * BUF_LINE_SIZE], mat, size);
  
  size_t cnt = s->mat_line_cnt;
  s->mat_line_cnt += size / (BUF_LINE_SIZE * sizeof(*s->mat_lines));

  return cnt;
}

obj *scn_get_obj(const scn *s, uint32_t idx)
{
  return &s->objs[idx];
}

void *scn_get_shape(const scn *s, uint32_t idx)
{
  return &s->shape_lines[idx * BUF_LINE_SIZE];
}

void *scn_get_mat(const scn *s, uint32_t idx)
{
  return &s->mat_lines[idx * BUF_LINE_SIZE];
}

aabb scn_get_obj_aabb(const scn *s, uint32_t idx)
{
  obj *o = scn_get_obj(s, idx);
  switch(o->shape_type) {
    case SPHERE:
      return sphere_get_aabb((sphere *)scn_get_shape(s, o->shape_idx));
      break;
    case QUAD:
      return quad_get_aabb((quad *)scn_get_shape(s, o->shape_idx));
      break;
    default:
      // Unknown or unsupported shape
      // TODO Log/alert
      return aabb_init();
  }
}

vec3 scn_get_obj_center(const scn *s, uint32_t idx)
{
  obj *o = scn_get_obj(s, idx);
  switch(o->shape_type) {
    case SPHERE:
      return ((sphere *)scn_get_shape(s, o->shape_idx))->center;
      break;
    case QUAD:
      return quad_get_center((quad *)scn_get_shape(s, o->shape_idx));
      break;
    default:
      // Unknown or unsupported shape
      // TODO Log/alert
      return (vec3){{{ 0.0f, 0.0f, 0.0f }}};
  }
}
