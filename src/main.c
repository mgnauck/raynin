#include "log.h"
#include "scene.h"

scene *curr_scene = NULL;

scene *create_scene_spheres()
{
  scene *s = init_scene(5, 5, 4);

  add_object(s, create_object(
        SPHERE, add_sphere(s, create_sphere((vec3){{{ 0.0f, -100.5f, 0.0f }}}, 100.0f)),
        LAMBERT, add_lambert(s, create_lambert((vec3){{{ 0.5f, 0.5f, 0.5f }}}))));

  add_object(s, create_object(
      SPHERE, add_sphere(s, create_sphere((vec3){{{ -1.0f, 0.0f, 0.0f }}}, 0.5f)),
      LAMBERT, add_lambert(s, create_lambert((vec3){{{ 0.6f, 0.3f, 0.3f }}}))));

  uint32_t glass_mat_ofs = add_glass(s, create_glass((vec3){{{ 1.0f, 1.0f, 1.0f }}}, 1.5f));

  add_object(s, create_object(SPHERE,
      add_sphere(s, create_sphere((vec3){{{ 0.0f, 0.0f, 0.0f }}}, 0.5f)),
      GLASS, glass_mat_ofs));

  add_object(s, create_object(
      SPHERE, add_sphere(s, create_sphere((vec3){{{ 0.0f, 0.0f, 0.0f }}}, -0.45f)),
      GLASS, glass_mat_ofs));

  add_object(s, create_object(
      SPHERE, add_sphere(s, create_sphere((vec3){{{ 1.0f, 0.0f, 0.0f }}}, 0.5f)),
      METAL, add_metal(s, create_metal((vec3){{{ 0.3f, 0.3f, 0.6f }}}, 0.0f))));

  return s;
}

__attribute__((visibility("default")))
uint32_t get_object_line_count()
{
  return curr_scene->object_line_count;
}

__attribute__((visibility("default")))
void *get_object_buf()
{
  return curr_scene->object_buf;
}

__attribute__((visibility("default")))
size_t get_object_buf_size()
{
  return curr_scene->object_line_count * OBJECT_LINE_SIZE * sizeof(*curr_scene->object_buf);
}

__attribute__((visibility("default")))
uint32_t get_shape_line_count()
{
  return curr_scene->shape_line_count;
}

__attribute__((visibility("default")))
void *get_shape_buf()
{
  return curr_scene->shape_buf;
}

__attribute__((visibility("default")))
size_t get_shape_buf_size()
{
  return curr_scene->shape_line_count * SHAPE_LINE_SIZE * sizeof(*curr_scene->shape_buf);
}

__attribute__((visibility("default")))
uint32_t get_material_line_count()
{
  return curr_scene->material_line_count;
}

__attribute__((visibility("default")))
void *get_material_buf()
{
  return curr_scene->material_buf;
}

__attribute__((visibility("default")))
size_t get_material_buf_size()
{
  return curr_scene->material_line_count * MATERIAL_LINE_SIZE * sizeof(*curr_scene->material_buf);
}

__attribute__((visibility("default")))
void init(void)
{
  log("init()");

  curr_scene = create_scene_spheres();
}

__attribute__((visibility("default")))
void release(void)
{
  log("release()");

  release_scene(curr_scene);
}
