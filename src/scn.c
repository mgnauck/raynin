#include "scn.h"
#include "sutil.h"
#include "shape.h"
#include "mat.h"

size_t scn_shape_capacity(size_t sphere_cnt, size_t quad_cnt)
{
  return sphere_cnt * sizeof(sphere) / (BUF_LINE_SIZE * sizeof(float)) +
    quad_cnt * sizeof(quad) / (BUF_LINE_SIZE * sizeof(float));
}

scn *scn_init(size_t obj_buf_capacity, size_t shape_buf_capacity, size_t mat_buf_capacity)
{
  scn *s = malloc(sizeof(*s));

  s->obj_buf = malloc(obj_buf_capacity * BUF_LINE_SIZE * sizeof(*s->obj_buf));
  s->obj_idx = 0;
  
  s->shape_buf = malloc(shape_buf_capacity * BUF_LINE_SIZE * sizeof(*s->shape_buf));
  s->shape_idx = 0;
  
  s->mat_buf = malloc(mat_buf_capacity * BUF_LINE_SIZE * sizeof(*s->mat_buf));
  s->mat_idx = 0;
  
  return s;
}

void scn_release(scn *s)
{
  free(s->obj_buf);
  free(s->shape_buf);
  free(s->mat_buf);
  free(s);
}

uint32_t scn_add_obj(scn *s, obj obj)
{
  memcpy(&s->obj_buf[s->obj_idx * BUF_LINE_SIZE], &obj, sizeof(obj));

  size_t idx = s->obj_idx;
  s->obj_idx += sizeof(obj) / (BUF_LINE_SIZE * sizeof(*s->obj_buf));
  
  return idx;
}

uint32_t scn_add_shape(scn *s, const void *shape, size_t size)
{
  memcpy(&s->shape_buf[s->shape_idx * BUF_LINE_SIZE], shape, size);
  
  size_t idx = s->shape_idx;
  s->shape_idx += size / (BUF_LINE_SIZE * sizeof(*s->shape_buf));

  return idx;
}

uint32_t scn_add_mat(scn *s, const void *mat, size_t size)
{
  memcpy(&s->mat_buf[s->mat_idx * BUF_LINE_SIZE], mat, size);
  
  size_t idx = s->mat_idx;
  s->mat_idx += size / (BUF_LINE_SIZE * sizeof(*s->mat_buf));

  return idx;
}

obj *scn_get_obj(const scn *s, uint32_t idx)
{
  return (obj *)&s->obj_buf[idx * BUF_LINE_SIZE];
}

void *scn_get_shape(const scn *s, uint32_t idx)
{
  return &s->shape_buf[idx * BUF_LINE_SIZE];
}

void *scn_get_mat(const scn *s, uint32_t idx)
{
  return &s->mat_buf[idx * BUF_LINE_SIZE];
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
