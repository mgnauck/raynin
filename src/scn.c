#include "scn.h"
#include "sutil.h"

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

uint32_t scn_add_sphere(scn *s, sphere shape)
{
  memcpy(&s->shape_buf[s->shape_idx * BUF_LINE_SIZE], &shape, sizeof(sphere));
  
  size_t idx = s->shape_idx;
  s->shape_idx += sizeof(sphere) / (BUF_LINE_SIZE * sizeof(*s->shape_buf));

  return idx;
}

uint32_t scn_add_quad(scn *s, quad shape)
{
  memcpy(&s->shape_buf[s->shape_idx * BUF_LINE_SIZE], &shape, sizeof(quad));
  
  size_t idx = s->shape_idx;
  s->shape_idx += sizeof(quad) / (BUF_LINE_SIZE * sizeof(*s->shape_buf));

  return idx;
}

uint32_t scn_add_lambert(scn *s, lambert mat)
{
  memcpy(&s->mat_buf[s->mat_idx * BUF_LINE_SIZE], &mat, sizeof(lambert));

  size_t idx = s->mat_idx;
  s->mat_idx += sizeof(lambert) / (BUF_LINE_SIZE * sizeof(*s->mat_buf));

  return idx;
}

uint32_t scn_add_metal(scn *s, metal mat)
{  
  memcpy(&s->mat_buf[s->mat_idx * BUF_LINE_SIZE], &mat, sizeof(metal));

  size_t idx = s->mat_idx;
  s->mat_idx += sizeof(metal) / (BUF_LINE_SIZE * sizeof(*s->mat_buf));

  return idx;
}

uint32_t scn_add_glass(scn *s, glass mat)
{  
  memcpy(&s->mat_buf[s->mat_idx * BUF_LINE_SIZE], &mat, sizeof(glass));

  size_t idx = s->mat_idx;
  s->mat_idx += sizeof(glass) / (BUF_LINE_SIZE * sizeof(*s->mat_buf));

  return idx;
}

uint32_t scn_add_emitter(scn *s, emitter mat)
{  
  memcpy(&s->mat_buf[s->mat_idx * BUF_LINE_SIZE], &mat, sizeof(emitter));

  size_t idx = s->mat_idx;
  s->mat_idx += sizeof(emitter) / (BUF_LINE_SIZE * sizeof(*s->mat_buf));

  return idx;
}

obj *scn_get_obj(const scn *s, uint32_t idx)
{
  return (obj *)&s->obj_buf[idx * BUF_LINE_SIZE];
}

aabb scn_get_obj_aabb(const scn *s, uint32_t idx)
{
  obj *o = scn_get_obj(s, idx);
  switch(o->shape_type) {
    case SPHERE:
      return sphere_get_aabb(scn_get_sphere(s, o->shape_idx));
      break;
    case QUAD:
      return quad_get_aabb(scn_get_quad(s, o->shape_idx));
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
      return scn_get_sphere(s, o->shape_idx)->center;
      break;
    case QUAD:
      return quad_get_center(scn_get_quad(s, o->shape_idx));
      break;
    default:
      // Unknown or unsupported shape
      // TODO Log/alert
      return (vec3){{{ 0.0f, 0.0f, 0.0f }}};
  }
}
sphere *scn_get_sphere(const scn *s, uint32_t idx)
{
  return (sphere *)&s->shape_buf[idx * BUF_LINE_SIZE];
}

quad *scn_get_quad(const scn* s, uint32_t idx)
{
  return (quad *)&s->shape_buf[idx * BUF_LINE_SIZE];
}

lambert *scn_get_lambert(const scn *s, uint32_t idx)
{
  return (lambert *)&s->mat_buf[idx * BUF_LINE_SIZE];
}

metal *scn_get_metal(const scn *s, uint32_t idx)
{
  return (metal *)&s->mat_buf[idx * BUF_LINE_SIZE];
}

glass *scn_get_glass(const scn *s, uint32_t idx)
{
  return (glass *)&s->mat_buf[idx * BUF_LINE_SIZE];
}

emitter *scn_get_emitter(const scn *s, uint32_t idx)
{
  return (emitter *)&s->mat_buf[idx * BUF_LINE_SIZE];
}
