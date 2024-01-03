#include "sutil.h"
#include "mutil.h"
#include "cfg.h"
#include "scn.h"
#include "bvh.h"
#include "cam.h"
#include "view.h"
#include "log.h"

#define GLOBALS_BUF_SIZE 32 * 4

cfg       config;
uint32_t  gathered_smpls = 0;

uint8_t   *globals_buf;
scn       *curr_scn;
bvh       *curr_bvh;

view      curr_view;
cam       curr_cam;

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

  curr_cam = (cam){ .vert_fov = 60.0f, .foc_dist = 3.0f, .foc_angle = 0.0f };
  cam_set(&curr_cam, (vec3){{{ 0.0f, 0.0f, 2.0f }}}, (vec3){{{ 0.0f, 0.0f, 0.0f }}});

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

  curr_cam = (cam){ .vert_fov = 680.0f, .foc_dist = 3.0f, .foc_angle = 0.0f };
  cam_set(&curr_cam, (vec3){{{ 0.0f, 0.0f, 9.0f }}}, (vec3){{{ 0.0f, 0.0f, 0.0f }}});

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

  curr_cam = (cam){ .vert_fov = 20.0f, .foc_dist = 10.0f, .foc_angle = 0.6f };
  cam_set(&curr_cam, (vec3){{{ 13.0f, 2.0f, 3.0f }}}, (vec3){{{ 0.0f, 0.0f, 0.0f }}});

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
void *get_globals_buf()
{
  return globals_buf;
}

__attribute__((visibility("default")))
size_t get_globals_buf_size()
{
  return GLOBALS_BUF_SIZE;
}

__attribute__((visibility("default")))
void init(uint32_t width, uint32_t height, uint32_t spp, uint32_t bounces)
{
  srand(42u, 54u);

  config = (cfg){ width, height, spp, bounces };

  double t = get_time();
  //curr_scn = create_scn_spheres();
  //curr_scn = create_scn_quads();
  curr_scn = create_scn_riow();
  t = get_time() - t;
  log("scn create: %2.3f, obj cnt: %d, shape lines: %d, mat lines: %d",
      t, curr_scn->obj_idx, curr_scn->shape_idx, curr_scn->mat_idx);

  t = get_time();
  curr_bvh = bvh_create(curr_scn);
  t = get_time() - t;
  log("bvh create: %2.3f ms, node cnt: %d", t, curr_bvh->node_cnt);
  
  globals_buf = malloc(GLOBALS_BUF_SIZE * sizeof(*globals_buf));

  view_calc(&curr_view, config.width, config.height, &curr_cam);
  cfg_write(globals_buf, &config);
}

__attribute__((visibility("default")))
void update(float time)
{
  // TODO Update cam etc.

  // Write globals data (except cfg)
  const float frame_data[4] = { randf(), (float)config.spp / (gathered_smpls + config.spp), time, 0.0f };
  memcpy(globals_buf + 4 * 4, frame_data, 4 * sizeof(float));
  size_t ofs = cam_write(globals_buf + 8 * 4, &curr_cam);
  view_write(globals_buf + 8 * 4 + ofs, &curr_view);

  gathered_smpls += config.spp;
}

__attribute__((visibility("default")))
void release(void)
{
  bvh_release(curr_bvh);
  scn_release(curr_scn);
  free(globals_buf);
}
