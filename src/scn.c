#include "scn.h"
#include "sutil.h"
#include "obj.h"
#include "shape.h"
#include "mat.h"

#define BUF_LINE_SIZE 4

size_t scn_calc_shape_buf_size(size_t sphere_cnt, size_t quad_cnt)
{
  return sphere_cnt * sizeof(sphere) + quad_cnt * sizeof(quad);
}

size_t scn_calc_mat_buf_size(size_t lambert_cnt, size_t metal_cnt,
    size_t glass_cnt, size_t emitter_cnt)
{
  return lambert_cnt * sizeof(lambert) + metal_cnt * sizeof(metal) +
    glass_cnt * sizeof(glass) + emitter_cnt * sizeof(emitter);
}

scn *scn_init(size_t obj_cnt, size_t shape_buf_size, size_t mat_buf_size)
{
  scn *s = malloc(sizeof(*s));

  s->objs = malloc(obj_cnt * sizeof(*s->objs));
  s->obj_cnt = 0;
  
  s->shape_buf = malloc(shape_buf_size);
  s->shape_buf_size = 0;
  
  s->mat_buf = malloc(mat_buf_size);
  s->mat_buf_size = 0;
  
  return s;
}

void scn_release(scn *s)
{
  free(s->objs);
  free(s->shape_buf);
  free(s->mat_buf);
  free(s);
}

void scn_add_obj(scn *s, const obj *obj)
{
  memcpy(s->objs + s->obj_cnt++, obj, sizeof(*obj));
}

size_t scn_add_shape(scn *s, const void *shape, size_t size)
{
  size_t ofs = s->shape_buf_size / (BUF_LINE_SIZE * sizeof(*s->shape_buf));
  memcpy(s->shape_buf + s->shape_buf_size / sizeof(*s->shape_buf), shape, size);
  s->shape_buf_size += size;
  return ofs;
}

size_t scn_add_mat(scn *s, const void *mat, size_t size)
{
  size_t ofs = s->mat_buf_size / (BUF_LINE_SIZE * sizeof(*s->mat_buf));
  memcpy(s->mat_buf + s->mat_buf_size / sizeof(*s->mat_buf), mat, size);
  s->mat_buf_size += size;
  return ofs;
}

obj *scn_get_obj(const scn *s, size_t idx)
{
  return &s->objs[idx];
}

void *scn_get_shape(const scn *s, size_t ofs)
{
  return &s->shape_buf[ofs * BUF_LINE_SIZE];
}

void *scn_get_mat(const scn *s, size_t ofs)
{
  return &s->mat_buf[ofs * BUF_LINE_SIZE];
}

aabb scn_get_obj_aabb(const scn *s, size_t idx)
{
  obj *o = scn_get_obj(s, idx);
  switch(o->shape_type) {
    case SPHERE:
      return sphere_get_aabb((sphere *)scn_get_shape(s, o->shape_ofs));
      break;
    case QUAD:
      return quad_get_aabb((quad *)scn_get_shape(s, o->shape_ofs));
      break;
    default:
      // Unknown or unsupported shape
      // TODO Log/alert
      return aabb_init();
  }
}

vec3 scn_get_obj_center(const scn *s, size_t idx)
{
  obj *o = scn_get_obj(s, idx);
  switch(o->shape_type) {
    case SPHERE:
      return ((sphere *)scn_get_shape(s, o->shape_ofs))->center;
      break;
    case QUAD:
      return quad_get_center((quad *)scn_get_shape(s, o->shape_ofs));
      break;
    default:
      // Unknown or unsupported shape
      // TODO Log/alert
      return (vec3){{{ 0.0f, 0.0f, 0.0f }}};
  }
}
