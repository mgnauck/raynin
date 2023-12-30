#include "log.h"
#include "mathutil.h"
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

scene *create_scene_quads()
{
  scene *s = init_scene(7, 17, 7);

  add_object(s, create_object(
        QUAD, add_quad(s, create_quad((vec3){{{ -3.0f, -2.0f, 5.0f }}}, (vec3){{{ 0.0f, 0.0f, -4.0f }}}, (vec3){{{ 0.0f, 4.0f, 0.0f }}})),
        LAMBERT, add_lambert(s, create_lambert((vec3){{{ 1.0f, 0.2f, 0.2f }}}))));
  add_object(s, create_object(
        QUAD, add_quad(s, create_quad((vec3){{{ -2.0f, -2.0f, 0.0f }}}, (vec3){{{ 4.0f, 0.0f, 0.0f }}}, (vec3){{{ 0.0f, 4.0f, 0.0f }}})),
        LAMBERT, add_lambert(s, create_lambert((vec3){{{ 0.2f, 1.0f, 0.2f }}}))));
  add_object(s, create_object(
        QUAD, add_quad(s, create_quad((vec3){{{ 3.0f, -2.0f, 1.0f }}}, (vec3){{{ 0.0f, 0.0f, 4.0f }}}, (vec3){{{ 0.0f, 4.0f, 0.0f }}})),
        LAMBERT, add_lambert(s, create_lambert((vec3){{{ 0.2f, 0.2f, 1.0f }}}))));
  add_object(s, create_object(
        QUAD, add_quad(s, create_quad((vec3){{{ -2.0f, 3.0f, 1.0f }}}, (vec3){{{ 4.0f, 0.0f, 0.0f }}}, (vec3){{{ 0.0f, 0.0f, 4.0f }}})),
        LAMBERT, add_lambert(s, create_lambert((vec3){{{ 1.0f, 0.5f, 0.0f }}}))));
  add_object(s, create_object(
        QUAD, add_quad(s, create_quad((vec3){{{ -2.0f, -3.0f, 5.0f }}}, (vec3){{{ 4.0f, 0.0f, 0.0f }}}, (vec3){{{ 0.0f, 0.0f, -4.0f }}})),
        LAMBERT, add_lambert(s, create_lambert((vec3){{{ 0.2f, 0.8f, 0.8f }}}))));

  add_object(s, create_object(SPHERE,
      add_sphere(s, create_sphere((vec3){{{ 0.0f, 0.0f, 2.5f }}}, 1.5f)),
      GLASS, add_glass(s, create_glass((vec3){{{ 1.0f, 1.0f, 1.0f }}}, 1.5f))));
  add_object(s, create_object(
      SPHERE, add_sphere(s, create_sphere((vec3){{{ 0.0f, 0.0f, 2.5f }}}, 1.0f)),
      LAMBERT, add_lambert(s, create_lambert((vec3){{{ 0.0f, 0.0f, 1.0f }}}))));

  return s;
}

scene *create_scene_riow()
{
#define SIZE 22
  scene *s = init_scene(SIZE * SIZE + 4, SIZE * SIZE + 4, SIZE * SIZE + 4);

  add_object(s, create_object(
        SPHERE, add_sphere(s, create_sphere((vec3){{{ 0.0f, -1000.0f, 0.0f}}}, 1000.0f)),
        LAMBERT, add_lambert(s, create_lambert((vec3){{{ 0.5f, 0.5f, 0.5f }}}))));
  add_object(s, create_object(
        SPHERE, add_sphere(s, create_sphere((vec3){{{ 0.0f, 1.0f, 0.0f}}}, 1.0f)),
        GLASS, add_glass(s, create_glass((vec3){{{ 1.0f, 1.0f, 1.0f }}}, 1.5f))));
  add_object(s, create_object(
        SPHERE, add_sphere(s, create_sphere((vec3){{{ -4.0f, 1.0f, 0.0f}}}, 1.0f)),
        LAMBERT, add_lambert(s, create_lambert((vec3){{{ 0.4f, 0.2f, 0.1f }}}))));
  add_object(s, create_object(
        SPHERE, add_sphere(s, create_sphere((vec3){{{ 4.0f, 1.0f, 0.0f}}}, 1.0f)),
        METAL, add_metal(s, create_metal((vec3){{{ 0.7f, 0.6f, 0.5f }}}, 0.0f))));

  for(int a=-SIZE/2; a<SIZE/2; a++) {
    for(int b=-SIZE/2; b<SIZE/2; b++) {
      float mat_p = randf();
      vec3 center = {{{ (float)a + 0.9f * randf(), 0.2f, (float)b + 0.9f * randf() }}};
      if(vec3_len(vec3_add(center, (vec3){{{ -4.0f, -0.2f, 0.0f}}})) > 0.9f) {
        uint32_t mat_type, mat_ofs;
        if(mat_p < 0.8f) {
          mat_type = LAMBERT;
          mat_ofs = add_lambert(s, create_lambert(vec3_mul(vec3_rand(), vec3_rand())));
        } else if(mat_p < 0.95f) {
          mat_type = METAL;
          mat_ofs = add_metal(s, create_metal(vec3_rand_rng(0.5f, 1.0f), randf_rng(0.0f, 0.5f)));
        } else {
          mat_type = GLASS;
          mat_ofs = add_glass(s, create_glass((vec3){{{ 1.0f, 1.0f, 1.0f }}}, 1.5f));
        }
        add_object(s, create_object(
              SPHERE, add_sphere(s, create_sphere(center, 0.2f)), mat_type, mat_ofs));
      }
    }
  }

  return s;
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

  srand(42u, 54u);

  curr_scene = create_scene_spheres();
  //curr_scene = create_scene_quads();
  //curr_scene = create_scene_riow();

  log("obj line cnt: %d, shape line cnt: %d, mat line cnt: %d",
      curr_scene->object_line_count,
      curr_scene->shape_line_count,
      curr_scene->material_line_count);
}

__attribute__((visibility("default")))
void release(void)
{
  log("release()");

  release_scene(curr_scene);
}
