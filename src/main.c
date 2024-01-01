#include "log.h"
#include "mutil.h"
#include "scn.h"
#include "bvh.h"

scn *curr_scn;
bvh *curr_bvh;

scn *create_scn_spheres()
{
  scn *s = scn_init(5, 5, 4);

  scn_add_obj(s, obj_create(
        SPHERE, scn_add_sphere(s,
          sphere_create((vec3){{{ 0.0f, -100.5f, 0.0f }}}, 100.0f)),
        LAMBERT, scn_add_lambert(s,
          lambert_create((vec3){{{ 0.5f, 0.5f, 0.5f }}}))));

  scn_add_obj(s, obj_create(
      SPHERE, scn_add_sphere(s,
        sphere_create((vec3){{{ -1.0f, 0.0f, 0.0f }}}, 0.5f)),
      LAMBERT, scn_add_lambert(s,
        lambert_create((vec3){{{ 0.6f, 0.3f, 0.3f }}}))));

  uint32_t glass_mat_idx = scn_add_glass(s,
      glass_create((vec3){{{ 1.0f, 1.0f, 1.0f }}}, 1.5f));

  scn_add_obj(s, obj_create(SPHERE,
      scn_add_sphere(s,
        sphere_create((vec3){{{ 0.0f, 0.0f, 0.0f }}}, 0.5f)),
      GLASS, glass_mat_idx));

  scn_add_obj(s, obj_create(
      SPHERE, scn_add_sphere(s,
        sphere_create((vec3){{{ 0.0f, 0.0f, 0.0f }}}, -0.45f)),
      GLASS, glass_mat_idx));

  scn_add_obj(s, obj_create(
      SPHERE, scn_add_sphere(s,
        sphere_create((vec3){{{ 1.0f, 0.0f, 0.0f }}}, 0.5f)),
      METAL, scn_add_metal(s,
        metal_create((vec3){{{ 0.3f, 0.3f, 0.6f }}}, 0.0f))));

  return s;
}

scn *create_scn_quads()
{
  scn *s = scn_init(7, 17, 7);

  scn_add_obj(s, obj_create(
        QUAD, scn_add_quad(s,
          quad_create(
            (vec3){{{ -3.0f, -2.0f, 5.0f }}},
            (vec3){{{ 0.0f, 0.0f, -4.0f }}},
            (vec3){{{ 0.0f, 4.0f, 0.0f }}})),
        LAMBERT, scn_add_lambert(s,
          lambert_create((vec3){{{ 1.0f, 0.2f, 0.2f }}}))));
  
  scn_add_obj(s, obj_create(
        QUAD, scn_add_quad(s,
          quad_create(
            (vec3){{{ -2.0f, -2.0f, 0.0f }}},
            (vec3){{{ 4.0f, 0.0f, 0.0f }}},
            (vec3){{{ 0.0f, 4.0f, 0.0f }}})),
        LAMBERT, scn_add_lambert(s,
          lambert_create((vec3){{{ 0.2f, 1.0f, 0.2f }}}))));
  
  scn_add_obj(s, obj_create(
        QUAD, scn_add_quad(s,
          quad_create(
            (vec3){{{ 3.0f, -2.0f, 1.0f }}},
            (vec3){{{ 0.0f, 0.0f, 4.0f }}},
            (vec3){{{ 0.0f, 4.0f, 0.0f }}})),
        LAMBERT, scn_add_lambert(s,
          lambert_create((vec3){{{ 0.2f, 0.2f, 1.0f }}}))));

  scn_add_obj(s, obj_create(
        QUAD, scn_add_quad(s,
          quad_create(
            (vec3){{{ -2.0f, 3.0f, 1.0f }}}, 
            (vec3){{{ 4.0f, 0.0f, 0.0f }}}, 
            (vec3){{{ 0.0f, 0.0f, 4.0f }}})),
        LAMBERT, scn_add_lambert(s,
          lambert_create((vec3){{{ 1.0f, 0.5f, 0.0f }}}))));
  
  scn_add_obj(s, obj_create(
        QUAD, scn_add_quad(s,
          quad_create(
            (vec3){{{ -2.0f, -3.0f, 5.0f }}},
            (vec3){{{ 4.0f, 0.0f, 0.0f }}},
            (vec3){{{ 0.0f, 0.0f, -4.0f }}})),
        LAMBERT, scn_add_lambert(s,
          lambert_create((vec3){{{ 0.2f, 0.8f, 0.8f }}}))));

  scn_add_obj(s, obj_create(SPHERE,
      scn_add_sphere(s,
        sphere_create((vec3){{{ 0.0f, 0.0f, 2.5f }}}, 1.5f)),
      GLASS, scn_add_glass(s,
        glass_create((vec3){{{ 1.0f, 1.0f, 1.0f }}}, 1.5f))));

  scn_add_obj(s, obj_create(
      SPHERE, scn_add_sphere(s,
        sphere_create((vec3){{{ 0.0f, 0.0f, 2.5f }}}, 1.0f)),
      LAMBERT, scn_add_lambert(s,
        lambert_create((vec3){{{ 0.0f, 0.0f, 1.0f }}}))));

  return s;
}

scn *create_scn_riow()
{
#define SIZE 22
  scn *s = scn_init(SIZE * SIZE + 4, SIZE * SIZE + 4, SIZE * SIZE + 4);

  scn_add_obj(s, obj_create(
        SPHERE, scn_add_sphere(s,
          sphere_create((vec3){{{ 0.0f, -1000.0f, 0.0f}}}, 1000.0f)),
        LAMBERT, scn_add_lambert(s,
          lambert_create((vec3){{{ 0.5f, 0.5f, 0.5f }}}))));
  
  scn_add_obj(s, obj_create(
        SPHERE, scn_add_sphere(s,
          sphere_create((vec3){{{ 0.0f, 1.0f, 0.0f}}}, 1.0f)),
        GLASS, scn_add_glass(s,
          glass_create((vec3){{{ 1.0f, 1.0f, 1.0f }}}, 1.5f))));
  
  scn_add_obj(s, obj_create(
        SPHERE, scn_add_sphere(s,
          sphere_create((vec3){{{ -4.0f, 1.0f, 0.0f}}}, 1.0f)),
        LAMBERT, scn_add_lambert(s,
          lambert_create((vec3){{{ 0.4f, 0.2f, 0.1f }}}))));
  
  scn_add_obj(s, obj_create(
        SPHERE, scn_add_sphere(s,
          sphere_create((vec3){{{ 4.0f, 1.0f, 0.0f}}}, 1.0f)),
        METAL, scn_add_metal(s,
          metal_create((vec3){{{ 0.7f, 0.6f, 0.5f }}}, 0.0f))));

  for(int a=-SIZE/2; a<SIZE/2; a++) {
    for(int b=-SIZE/2; b<SIZE/2; b++) {
      float mat_p = randf();
      vec3 center = {{{
        (float)a + 0.9f * randf(), 0.2f, (float)b + 0.9f * randf() }}};
      if(vec3_len(vec3_add(center, (vec3){{{ -4.0f, -0.2f, 0.0f}}})) > 0.9f) {
        uint32_t mat_type, mat_idx;
        if(mat_p < 0.8f) {
          mat_type = LAMBERT;
          mat_idx = scn_add_lambert(s,
              lambert_create(vec3_mul(vec3_rand(), vec3_rand())));
        } else if(mat_p < 0.95f) {
          mat_type = METAL;
          mat_idx = scn_add_metal(s,
              metal_create(vec3_rand_rng(0.5f, 1.0f), randf_rng(0.0f, 0.5f)));
        } else {
          mat_type = GLASS;
          mat_idx = scn_add_glass(s,
              glass_create((vec3){{{ 1.0f, 1.0f, 1.0f }}}, 1.5f));
        }
        scn_add_obj(s, obj_create(
              SPHERE, scn_add_sphere(s,
                sphere_create(center, 0.2f)), mat_type, mat_idx));
      }
    }
  }

  return s;
}

__attribute__((visibility("default")))
void *get_obj_buf()
{
  return curr_scn->obj_buf;
}

__attribute__((visibility("default")))
size_t get_obj_buf_size()
{
  return curr_scn->obj_idx * BUF_LINE_SIZE * sizeof(*curr_scn->obj_buf);
}

__attribute__((visibility("default")))
void *get_shape_buf()
{
  return curr_scn->shape_buf;
}

__attribute__((visibility("default")))
size_t get_shape_buf_size()
{
  return curr_scn->shape_idx * BUF_LINE_SIZE * sizeof(*curr_scn->shape_buf);
}

__attribute__((visibility("default")))
void *get_mat_buf()
{
  return curr_scn->mat_buf;
}

__attribute__((visibility("default")))
size_t get_mat_buf_size()
{
  return curr_scn->mat_idx * BUF_LINE_SIZE * sizeof(*curr_scn->mat_buf);
}

__attribute__((visibility("default")))
void *get_bvh_node_buf()
{
  return curr_bvh->node_buf;
}

__attribute__((visibility("default")))
size_t get_bvh_node_buf_size()
{
  return curr_bvh->node_cnt * sizeof(*curr_bvh->node_buf);
}

__attribute__((visibility("default")))
void init(void)
{
  log("init()");

  srand(42u, 54u);

  curr_scn = create_scn_spheres();
  //curr_scn = create_scn_quads();
  //curr_scn = create_scn_riow();

  log("obj idx: %d, shape idx: %d, mat idx: %d",
      curr_scn->obj_idx,
      curr_scn->shape_idx,
      curr_scn->mat_idx);

  double t = get_time();
  curr_bvh = bvh_create(curr_scn);
  t = get_time() - t;

  log("bvh creation took: %6.3f ms", t);
  log("bvh node cnt: %d", curr_bvh->node_cnt);
}

__attribute__((visibility("default")))
void release(void)
{
  log("release()");

  bvh_release(curr_bvh);
  scn_release(curr_scn);
}
