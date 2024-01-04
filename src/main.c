#include <stdbool.h>
#include "sutil.h"
#include "gutil.h"
#include "mutil.h"
#include "cfg.h"
#include "scn.h"
#include "bvh.h"
#include "cam.h"
#include "view.h"
#include "log.h"

cfg       config;
uint32_t  gathered_smpls = 0;

scn       *curr_scn;
bvh       *curr_bvh;

view      curr_view;
cam       curr_cam;

bool      orbit_cam = false;

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

void update_cam_view()
{
  view_calc(&curr_view, config.width, config.height, &curr_cam);

  gpu_write_buf(GLOB, 32, &curr_cam, CAM_BUF_SIZE);
  gpu_write_buf(GLOB, 80, &curr_view, sizeof(view));
  
  gathered_smpls = TEMPORAL_WEIGHT * config.spp;
}

__attribute__((visibility("default")))
void init(uint32_t width, uint32_t height, uint32_t spp, uint32_t bounces)
{
  srand(42u, 303u);

  config = (cfg){ width, height, spp, bounces };

  curr_scn = create_scn_riow();
  curr_bvh = bvh_create(curr_scn);
  
  gpu_create_res(
      GLOB_BUF_SIZE,
      curr_bvh->node_cnt * sizeof(*curr_bvh->node_buf),
      curr_scn->obj_idx * BUF_LINE_SIZE * sizeof(*curr_scn->obj_buf),
      curr_scn->shape_idx * BUF_LINE_SIZE * sizeof(*curr_scn->shape_buf),
      curr_scn->mat_idx * BUF_LINE_SIZE * sizeof(*curr_scn->mat_buf));

  gpu_write_buf(BVH, 0, curr_bvh->node_buf, curr_bvh->node_cnt * sizeof(*curr_bvh->node_buf));
  gpu_write_buf(OBJ, 0, curr_scn->obj_buf, curr_scn->obj_idx * BUF_LINE_SIZE * sizeof(*curr_scn->obj_buf));
  gpu_write_buf(SHAPE, 0, curr_scn->shape_buf, curr_scn->shape_idx * BUF_LINE_SIZE * sizeof(*curr_scn->shape_buf));
  gpu_write_buf(MAT, 0, curr_scn->mat_buf, curr_scn->mat_idx * BUF_LINE_SIZE * sizeof(*curr_scn->mat_buf));

  gpu_write_buf(GLOB, 0, &config, sizeof(cfg));

  update_cam_view();
}

__attribute__((visibility("default")))
void update(float time)
{
  if(orbit_cam)
  {
    float s = 0.3f;
    float r = 15.0f;
    float h = 2.5f;

    cam_set(&curr_cam, (vec3){{{
        r * sinf(time * s),
        h + 0.75 * h * sinf(time * s * 0.7f),
        r * cosf(time *s) }}},
        vec3_unit((vec3){{{ 13.0f, 2.0f, 3.0f }}}));

    update_cam_view();    
  }

  float frame[4] = {
    randf(), config.spp / (float)(gathered_smpls + config.spp), time, 0.0f };
  gpu_write_buf(GLOB, 16, frame, 4 * sizeof(float));

  gathered_smpls += config.spp;
}

__attribute__((visibility("default")))
void release(void)
{
  bvh_release(curr_bvh);
  scn_release(curr_scn);
}

__attribute__((visibility("default")))
void key_down(unsigned char key)
{
  #define MOVE_VEL 0.1f
  
  switch(key) {
    case 'a':
      curr_cam.eye = vec3_add(curr_cam.eye, vec3_scale(curr_cam.right, -MOVE_VEL));
      break;
    case 'd':
      curr_cam.eye = vec3_add(curr_cam.eye, vec3_scale(curr_cam.right, MOVE_VEL));
      break;
    case 'w':
      curr_cam.eye = vec3_add(curr_cam.eye, vec3_scale(curr_cam.fwd, -MOVE_VEL));
     break;
    case 's':
      curr_cam.eye = vec3_add(curr_cam.eye, vec3_scale(curr_cam.fwd, MOVE_VEL));
      break;
    case 'i':
      curr_cam.foc_dist += 0.1f;
      break;
    case 'k':
      curr_cam.foc_dist = max(curr_cam.foc_dist - 0.1f, 0.1f);
      break;
    case 'j':
      curr_cam.foc_angle = max(curr_cam.foc_angle - 0.1f, 0.1f);
      break;
    case 'l':
      curr_cam.foc_angle += 0.1f;
      break;
    case 'o':
      orbit_cam = !orbit_cam;
      break;
    case 'r':
      // TODO hot reload shader
      break;
  }

  update_cam_view();
}

__attribute__((visibility("default")))
void mouse_move(int32_t dx, int32_t dy)
{
  #define LOOK_VELO 0.015f
  
  float theta = min(max(curr_cam.theta + (float)dy * LOOK_VELO, 0.01f), 0.99f * PI);
  float phi = fmodf(curr_cam.phi - (float)dx * LOOK_VELO, 2.0f * PI);

  cam_set_dir(&curr_cam, theta, phi);

  update_cam_view();
}
