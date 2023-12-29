#include "scene.h"
#include "util.h"

object create_object(uint32_t shape_type, uint32_t shape_ofs, uint32_t mat_type, uint32_t mat_ofs)
{
  return (object){
    .shape_type = shape_type,
    .shape_ofs = shape_ofs,
    .mat_type = mat_type,
    .mat_ofs = mat_ofs };
}

sphere create_sphere(vec3 center, float radius)
{
  return (sphere){ .center = center, .radius = radius };
}

quad create_quad(vec3 q, vec3 u, vec3 v)
{
  return (quad){ .q = q, .u = u, .v = v };
}

lambert create_lambert(vec3 albedo)
{
  return (lambert){ .albedo = albedo };
}

metal create_metal(vec3 albedo, float fuzz_radius)
{
  return (metal){ .albedo = albedo, .fuzz_radius = fuzz_radius };
}

glass create_glass(vec3 albedo, float refr_idx)
{
  return (glass){ .albedo = albedo, .refr_idx = refr_idx };
}

scene *init_scene(size_t obj_line_capacity, size_t shape_line_capacity, size_t mat_line_capacity)
{
  scene *s = malloc(sizeof(*s));

  s->object_buf = malloc(obj_line_capacity * OBJECT_LINE_SIZE * sizeof(*s->object_buf));
  s->object_line_count = 0;
  
  s->shape_buf = malloc(shape_line_capacity * SHAPE_LINE_SIZE * sizeof(*s->shape_buf));
  s->shape_line_count = 0;
  
  s->material_buf = malloc(mat_line_capacity * MATERIAL_LINE_SIZE * sizeof(*s->material_buf));
  s->material_line_count = 0;
  
  return s;
}

void release_scene(scene *s)
{
  free(s->object_buf);
  free(s->shape_buf);
  free(s->material_buf);
  free(s);
}

uint32_t add_object(scene *scene, object obj)
{
  memcpy(&scene->object_buf[scene->object_line_count * OBJECT_LINE_SIZE], &obj, sizeof(object));

  size_t ofs = scene->object_line_count;
  scene->object_line_count += sizeof(object) / (OBJECT_LINE_SIZE * sizeof(*scene->object_buf));
  
  return ofs;
}

uint32_t add_sphere(scene *scene, sphere shape)
{
  memcpy(&scene->shape_buf[scene->shape_line_count * SHAPE_LINE_SIZE], &shape, sizeof(sphere));
  
  size_t ofs = scene->shape_line_count;
  scene->shape_line_count += sizeof(sphere) / (SHAPE_LINE_SIZE * sizeof(*scene->shape_buf));

  return ofs;
}

uint32_t add_quad(scene *scene, quad shape)
{  
  memcpy(&scene->shape_buf[scene->shape_line_count * SHAPE_LINE_SIZE], &shape, sizeof(quad));
  
  size_t ofs = scene->shape_line_count;
  scene->shape_line_count += sizeof(quad) / (SHAPE_LINE_SIZE * sizeof(*scene->shape_buf));

  return ofs;
}

uint32_t add_lambert(scene *scene, lambert mat)
{
  memcpy(&scene->material_buf[scene->material_line_count * MATERIAL_LINE_SIZE], &mat, sizeof(lambert));

  size_t ofs = scene->material_line_count;
  scene->material_line_count += sizeof(lambert) / (MATERIAL_LINE_SIZE * sizeof(*scene->material_buf));

  return ofs;
}

uint32_t add_metal(scene *scene, metal mat)
{  
  memcpy(&scene->material_buf[scene->material_line_count * MATERIAL_LINE_SIZE], &mat, sizeof(metal));

  size_t ofs = scene->material_line_count;
  scene->material_line_count += sizeof(metal) / (MATERIAL_LINE_SIZE * sizeof(*scene->material_buf));

  return ofs;
}

uint32_t add_glass(scene *scene, glass mat)
{  
  memcpy(&scene->material_buf[scene->material_line_count * MATERIAL_LINE_SIZE], &mat, sizeof(glass));

  size_t ofs = scene->material_line_count;
  scene->material_line_count += sizeof(glass) / (MATERIAL_LINE_SIZE * sizeof(*scene->material_buf));

  return ofs;
}

object *get_object(scene *scene, uint32_t line_index)
{
  return (object *)&scene->object_buf[line_index * OBJECT_LINE_SIZE];
}

sphere *get_sphere(scene *scene, uint32_t line_index)
{
  return (sphere *)&scene->shape_buf[line_index * SHAPE_LINE_SIZE];
}

quad *get_quad(scene* scene, uint32_t line_index)
{
  return (quad *)&scene->shape_buf[line_index * SHAPE_LINE_SIZE];
}

lambert *get_lambert(scene *scene, uint32_t line_index)
{
  return (lambert *)&scene->material_buf[line_index * MATERIAL_LINE_SIZE];
}

metal *get_metal(scene *scene, uint32_t line_index)
{
  return (metal *)&scene->material_buf[line_index * MATERIAL_LINE_SIZE];
}

glass *get_glass(scene *scene, uint32_t line_index)
{
  return (glass *)&scene->material_buf[line_index * MATERIAL_LINE_SIZE];
}
