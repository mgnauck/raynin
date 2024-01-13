#include <stdbool.h>
#include "sutil.h"
#include "gutil.h"
#include "mutil.h"
#include "cfg.h"
#include "scn.h"
#include "obj.h"
#include "shape.h"
#include "mat.h"
#include "bvh.h"
#include "cam.h"
#include "view.h"
#include "log.h"

cfg       config;
uint32_t  gathered_smpls = 0;

scn       *curr_scn;
bvh       *curr_bvh;

vec3      bg_col = { 0.7f, 0.8f, 1.0f };
//vec3      bg_col = { 0.0f, 0.0f, 0.0f };
view      curr_view;
cam       curr_cam;

bool      orbit_cam = false;

void add_box(scn *s, vec3 a, vec3 b, size_t mat_type, size_t mat_ofs)
{
  vec3 mi = { min(a.x, b.x), min(a.y, b.y), min(a.z, b.z) };
  vec3 ma = { max(a.x, b.x), max(a.y, b.y), max(a.z, b.z) };

  vec3 dx = { ma.x - mi.x, 0.0f, 0.0f };
  vec3 dy = { 0.0f, ma.y - mi.y, 0.0f };
  vec3 dz = { 0.0f, 0.0f, ma.z - mi.z };

  scn_add_obj(s, &(obj){ QUAD,
      scn_add_shape(s, &(quad){ .q = (vec3){ mi.x, mi.y, ma.z }, .u = dx, .v = dy }, sizeof(quad)),
      mat_type, mat_ofs });
  scn_add_obj(s, &(obj){ QUAD,
      scn_add_shape(s, &(quad){ .q = (vec3){ ma.x, mi.y, ma.z }, .u = vec3_neg(dz), .v = dy }, sizeof(quad)),
      mat_type, mat_ofs });
  scn_add_obj(s, &(obj){ QUAD,
      scn_add_shape(s, &(quad){ .q = (vec3){ ma.x, mi.y, mi.z }, .u = vec3_neg(dx), .v = dy }, sizeof(quad)),
      mat_type, mat_ofs });
  scn_add_obj(s, &(obj){ QUAD,
      scn_add_shape(s, &(quad){ .q = (vec3){ mi.x, mi.y, mi.z }, .u = dz, .v = dy }, sizeof(quad)),
      mat_type, mat_ofs });
  scn_add_obj(s, &(obj){ QUAD,
      scn_add_shape(s, &(quad){ .q = (vec3){ mi.x, ma.y, ma.z }, .u = dx, .v = vec3_neg(dz) }, sizeof(quad)),
      mat_type, mat_ofs });
  scn_add_obj(s, &(obj){ QUAD,
      scn_add_shape(s, &(quad){ .q = (vec3){ mi.x, mi.y, mi.z }, .u = dx, .v = dz }, sizeof(quad)),
      mat_type, mat_ofs });
}

scn *create_scn_spheres()
{
  scn *s = scn_init(5, scn_calc_shape_buf_size(5, 0), scn_calc_mat_buf_size(2, 1, 1));

  scn_add_obj(s, &(obj){ 
        SPHERE, scn_add_shape(s,
          &(sphere){ (vec3){ 0.0f, -100.5f, 0.0f }, 100.0f }, sizeof(sphere)),
        LAMBERT, scn_add_mat(s,
          &(basic){ .albedo = (vec3){ 0.5f, 0.5f, 0.5f } }, sizeof(basic)) });

  scn_add_obj(s, &(obj){ 
      SPHERE, scn_add_shape(s,
        &(sphere){ (vec3){ -1.0f, 0.0f, 0.0f }, 0.5f }, sizeof(sphere)),
      LAMBERT, scn_add_mat(s,
        &(basic){ .albedo = (vec3){ 0.6f, 0.3f, 0.3f } }, sizeof(basic)) });

  size_t glass_mat = scn_add_mat(s,
      &(glass){ (vec3){ 1.0f, 1.0f, 1.0f }, 1.5f }, sizeof(glass));

  scn_add_obj(s, &(obj){ SPHERE,
      scn_add_shape(s,
        &(sphere){ (vec3){ 0.0f, 0.0f, 0.0f }, 0.5f }, sizeof(sphere)),
      GLASS, glass_mat });

  scn_add_obj(s, &(obj){ 
      SPHERE, scn_add_shape(s,
        &(sphere){ (vec3){ 0.0f, 0.0f, 0.0f }, -0.45f }, sizeof(sphere)),
      GLASS, glass_mat });

  scn_add_obj(s, &(obj){ 
      SPHERE, scn_add_shape(s,
        &(sphere){ (vec3){ 1.0f, 0.0f, 0.0f }, 0.5f }, sizeof(sphere)),
      METAL, scn_add_mat(s,
        &(metal){ (vec3){ 0.3f, 0.3f, 0.6f }, 0.0f }, sizeof(metal)) });

  curr_cam = (cam){ .vert_fov = 60.0f, .foc_dist = 3.0f, .foc_angle = 0.0f };
  cam_set(&curr_cam, (vec3){ 0.0f, 0.0f, 2.0f }, (vec3){ 0.0f, 0.0f, 0.0f });

  return s;
}

scn *create_scn_quads()
{
  scn *s = scn_init(7, scn_calc_shape_buf_size(2, 5), scn_calc_mat_buf_size(6, 0, 1));

  scn_add_obj(s, &(obj){ 
        QUAD, scn_add_shape(s,
          &(quad){
            .q = (vec3){ -3.0f, -2.0f, 5.0f },
            .u = (vec3){ 0.0f, 0.0f, -4.0f },
            .v = (vec3){ 0.0f, 4.0f, 0.0f } }, sizeof(quad)),
        LAMBERT, scn_add_mat(s,
          &(basic){ .albedo = (vec3){ 1.0f, 0.2f, 0.2f } }, sizeof(basic)) });
  
  scn_add_obj(s, &(obj){ 
        QUAD, scn_add_shape(s,
          &(quad){
            .q = (vec3){ -2.0f, -2.0f, 0.0f },
            .u = (vec3){ 4.0f, 0.0f, 0.0f },
            .v = (vec3){ 0.0f, 4.0f, 0.0f } }, sizeof(quad)),
        LAMBERT, scn_add_mat(s,
          &(basic){ .albedo = (vec3){ 0.2f, 1.0f, 0.2f } }, sizeof(basic)) });
  
  scn_add_obj(s, &(obj){ 
        QUAD, scn_add_shape(s,
          &(quad){
            .q = (vec3){ 3.0f, -2.0f, 1.0f },
            .u = (vec3){ 0.0f, 0.0f, 4.0f },
            .v = (vec3){ 0.0f, 4.0f, 0.0f } }, sizeof(quad)),
        LAMBERT, scn_add_mat(s,
          &(basic){ .albedo = (vec3){ 0.2f, 0.2f, 1.0f } }, sizeof(basic)) });

  scn_add_obj(s, &(obj){ 
        QUAD, scn_add_shape(s,
          &(quad){
            .q = (vec3){ -2.0f, 3.0f, 1.0f }, 
            .u = (vec3){ 4.0f, 0.0f, 0.0f }, 
            .v = (vec3){ 0.0f, 0.0f, 4.0f } }, sizeof(quad)),
        LAMBERT, scn_add_mat(s,
          &(basic){ .albedo = (vec3){ 1.0f, 0.5f, 0.0f } }, sizeof(basic)) });
  
  scn_add_obj(s, &(obj){ 
        QUAD, scn_add_shape(s,
          &(quad){
            .q = (vec3){ -2.0f, -3.0f, 5.0f },
            .u = (vec3){ 4.0f, 0.0f, 0.0f },
            .v = (vec3){ 0.0f, 0.0f, -4.0f } }, sizeof(quad)),
        LAMBERT, scn_add_mat(s,
          &(basic){ .albedo = (vec3){ 0.2f, 0.8f, 0.8f } }, sizeof(basic)) });

  scn_add_obj(s, &(obj){ SPHERE,
      scn_add_shape(s,
        &(sphere){ (vec3){ 0.0f, 0.0f, 2.5f }, 1.5f }, sizeof(sphere)),
      GLASS, scn_add_mat(s,
        &(glass){ (vec3){ 1.0f, 1.0f, 1.0f }, 1.5f }, sizeof(glass)) });

  scn_add_obj(s, &(obj){ 
      SPHERE, scn_add_shape(s,
        &(sphere){ (vec3){ 0.0f, 0.0f, 2.5f }, 1.0f }, sizeof(sphere)),
      LAMBERT, scn_add_mat(s,
        &(basic){ .albedo = (vec3){ 0.0f, 0.0f, 1.0f } }, sizeof(basic)) });

  curr_cam = (cam){ .vert_fov = 60.0f, .foc_dist = 3.0f, .foc_angle = 0.0f };
  cam_set(&curr_cam, (vec3){ 0.0f, 0.0f, 9.0f }, (vec3){ 0.0f, 0.0f, 0.0f });

  return s;
}

scn *create_scn_emitter()
{
  scn *s = scn_init(13, scn_calc_shape_buf_size(6, 7), scn_calc_mat_buf_size(4, 1, 1));

  size_t lmat = scn_add_mat(s, &(basic){ .albedo = (vec3){ 0.5f, 0.5f, 0.5f } }, sizeof(basic));
  scn_add_obj(s, &(obj){ 
        SPHERE, scn_add_shape(s,
          &(sphere){ (vec3){ 0.0f, -1000.0f, 0.0f }, 1000.0f }, sizeof(sphere)),
        LAMBERT, lmat });
 
  scn_add_obj(s, &(obj){
        SPHERE, scn_add_shape(s,
          &(sphere){ (vec3){ -5.0f, 3.0f, 3.0f }, 1.0f }, sizeof(sphere)),
        LAMBERT, scn_add_mat(s, &(basic){ .albedo = (vec3){ 0.0f, 1.0f, 0.0f } }, sizeof(basic)) });
 
  size_t mmat = scn_add_mat(s, &(metal){ (vec3){ 0.5f, 0.5f, 0.6f }, 0.0 }, sizeof(metal));
  scn_add_obj(s, &(obj){
        SPHERE, scn_add_shape(s,
          &(sphere){ (vec3){ -5.0f, 4.0f, 0.0f }, 2.0f }, sizeof(sphere)),
        METAL, mmat });
 
  scn_add_obj(s, &(obj){
        SPHERE, scn_add_shape(s,
          &(sphere){ (vec3){ -5.0f, 3.0f, -3.0f }, 1.0f }, sizeof(sphere)),
        LAMBERT, scn_add_mat(s, &(basic){ .albedo = (vec3){ 1.0f, 0.0f, 0.0f } }, sizeof(basic)) });
  
  size_t gmat = scn_add_mat(s, &(glass){ (vec3){ 1.0f, 1.0f, 1.0f }, 1.5f }, sizeof(glass));
  scn_add_obj(s, &(obj){ 
        SPHERE, scn_add_shape(s,
          &(sphere){ (vec3){ 0.0f, 2.0f, 0.0f }, 2.0f }, sizeof(sphere)),
        GLASS, gmat });

  size_t emat = scn_add_mat(s, &(basic){ .albedo = (vec3){ 4.0f, 4.0f, 4.0f } }, sizeof(basic));
  scn_add_obj(s, &(obj){ 
        SPHERE, scn_add_shape(s,
          &(sphere){ (vec3){ 0.0f, 7.0f, 0.0f }, 2.0f }, sizeof(sphere)),
        EMITTER, emat });
 
  scn_add_obj(s, &(obj){ 
        QUAD, scn_add_shape(s, &(quad){ 
          .q = (vec3){ 3.0f, 1.0f, -2.0f },
          .u = (vec3){ 2.0f, 0.0f, 0.0f },
          .v = (vec3){ 0.0f, 2.0f, 0.0f } }, sizeof(quad)),
        EMITTER, emat });

  add_box(s, (vec3){ -2.0f, 0.5f, 3.0f }, (vec3){ 2.0f, 2.5f, 3.5f }, METAL, mmat);
  //add_box(s, (vec3){ -30.0f, -30.0f, -30.0f }, (vec3){ 30.0f, 30.5f, 30.0f }, METAL, mmat);

  curr_cam = (cam){ .vert_fov = 20.0f, .foc_dist = 3.0f, .foc_angle = 0.0f };
  cam_set(&curr_cam, (vec3){ 26.0f, 3.0f, 6.0f }, (vec3){ 0.0f, 2.0f, 0.0f });

  return s;
}

scn *create_scn_riow()
{
#define SIZE 22
  scn *s = scn_init(SIZE * SIZE + 4, scn_calc_shape_buf_size(SIZE * SIZE + 4, 0),
      scn_calc_mat_buf_size(SIZE * SIZE + 4, 0, 0));

  scn_add_obj(s, &(obj){ 
        SPHERE, scn_add_shape(s,
          &(sphere){ (vec3){ 0.0f, -1000.0f, 0.0f }, 1000.0f }, sizeof(sphere)),
        LAMBERT, scn_add_mat(s,
          &(basic){ .albedo = (vec3){ 0.5f, 0.5f, 0.5f } }, sizeof(basic)) });
 
  scn_add_obj(s, &(obj){ 
        SPHERE, scn_add_shape(s,
          &(sphere){ (vec3){ 4.0f, 1.0f, 0.0f }, 1.0f }, sizeof(sphere)),
        METAL, scn_add_mat(s,
          &(metal){ (vec3){ 0.7f, 0.6f, 0.5f }, 0.0f }, sizeof(metal)) });

  scn_add_obj(s, &(obj){ 
        SPHERE, scn_add_shape(s,
          &(sphere){ (vec3){ 0.0f, 1.0f, 0.0f }, 1.0f }, sizeof(sphere)),
        GLASS, scn_add_mat(s,
          &(glass){ (vec3){ 1.0f, 1.0f, 1.0f }, 1.5f }, sizeof(glass)) });
  
  scn_add_obj(s, &(obj){ 
        SPHERE, scn_add_shape(s,
          &(sphere){ (vec3){ -4.0f, 1.0f, 0.0f }, 1.0f }, sizeof(sphere)),
        LAMBERT, scn_add_mat(s,
          &(basic){ .albedo = (vec3){ 0.4f, 0.2f, 0.1f } }, sizeof(basic)) });
  
   for(int a=-SIZE/2; a<SIZE/2; a++) {
    for(int b=-SIZE/2; b<SIZE/2; b++) {
      float mat_p = randf();
      vec3 center = {
        (float)a + 0.9f * randf(), 0.2f, (float)b + 0.9f * randf() };
      if(vec3_len(vec3_add(center, (vec3){ -4.0f, -0.2f, 0.0f })) > 0.9f) {
        size_t t, m;
        if(mat_p < 0.8f) {
          t = LAMBERT;
          m = scn_add_mat(s,
              &(basic){ .albedo = vec3_mul(vec3_rand(), vec3_rand()) }, sizeof(basic));
        } else if(mat_p < 0.95f) {
          t = METAL;
          m = scn_add_mat(s,
              &(metal){ vec3_rand_rng(0.5f, 1.0f), randf_rng(0.0f, 0.5f) }, sizeof(metal));
        } else {
          t = GLASS;
          m = scn_add_mat(s,
              &(glass){ (vec3){ 1.0f, 1.0f, 1.0f }, 1.5f }, sizeof(glass));
        }
        scn_add_obj(s, &(obj){ 
              SPHERE, scn_add_shape(s,
                &(sphere){ center, 0.2f }, sizeof(sphere)), t, m });
      }
    }
  }

  curr_cam = (cam){ .vert_fov = 20.0f, .foc_dist = 10.0f, .foc_angle = 0.6f };
  cam_set(&curr_cam, (vec3){ 13.0f, 2.0f, 3.0f }, (vec3){ 0.0f, 0.0f, 0.0f });

  return s;
}

void update_cam_view()
{
  view_calc(&curr_view, config.width, config.height, &curr_cam);

  gpu_write_buf(GLOB, GLOB_BUF_OFS_CAM, &curr_cam, CAM_BUF_SIZE);
  gpu_write_buf(GLOB, GLOB_BUF_OFS_VIEW, &curr_view, sizeof(view));
  
  gathered_smpls = TEMPORAL_WEIGHT * config.spp;
}

__attribute__((visibility("default")))
void init(uint32_t width, uint32_t height)
{
  srand(42u, 303u);

  config = (cfg){ width, height, 5, 5 };

  curr_scn = create_scn_riow();

  curr_bvh = bvh_init(curr_scn->obj_cnt);
  bvh_create(curr_bvh, curr_scn);

  gpu_create_res(
      GLOB_BUF_SIZE,
      curr_bvh->node_cnt * sizeof(*curr_bvh->nodes),
      curr_scn->obj_cnt * sizeof(*curr_bvh->indices),
      curr_scn->obj_cnt * sizeof(*curr_scn->objs),
      curr_scn->shape_buf_size, curr_scn->mat_buf_size);

  gpu_write_buf(BVH, 0, curr_bvh->nodes, curr_bvh->node_cnt * sizeof(*curr_bvh->nodes));
  gpu_write_buf(INDEX, 0, curr_bvh->indices, curr_scn->obj_cnt * sizeof(*curr_bvh->indices));
  gpu_write_buf(OBJ, 0, curr_scn->objs, curr_scn->obj_cnt * sizeof(*curr_scn->objs));
  gpu_write_buf(SHAPE, 0, curr_scn->shape_buf, curr_scn->shape_buf_size);
  gpu_write_buf(MAT, 0, curr_scn->mat_buf, curr_scn->mat_buf_size);

  gpu_write_buf(GLOB, GLOB_BUF_OFS_CFG, &config, sizeof(cfg));

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

    vec3 pos = (vec3){ r * sinf(time * s), h + 0.75 * h * sinf(time * s * 0.7f), r * cosf(time *s) };
    vec3 tgt = vec3_unit((vec3){ 13.0f, 2.0f, 3.0f });
    cam_set(&curr_cam, pos, tgt);

    update_cam_view();
  }

  float frame[8] = { randf(), config.spp / (float)(gathered_smpls + config.spp),
    time, 0.0f, bg_col.x, bg_col.y, bg_col.z, 0.0f };
  gpu_write_buf(GLOB, GLOB_BUF_OFS_FRAME, frame, 8 * sizeof(float));

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
      // TODO Call JS to reload shader
      break;
  }

  update_cam_view();
}

__attribute__((visibility("default")))
void mouse_move(int32_t dx, int32_t dy)
{
  #define LOOK_VEL 0.015f

  float theta = min(max(acosf(-curr_cam.fwd.y) + (float)dy * LOOK_VEL, 0.01f), 0.99f * PI);
  float phi = fmodf(atan2f(-curr_cam.fwd.z, curr_cam.fwd.x) + PI - (float)dx * LOOK_VEL, 2.0f * PI);
  
  cam_set_dir(&curr_cam, vec3_spherical(theta, phi));

  update_cam_view();
}
