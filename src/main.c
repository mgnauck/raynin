#include <stdbool.h>
#include "sutil.h"
#include "gutil.h"
#include "mutil.h"
#include "cfg.h"
#include "cam.h"
#include "view.h"
#include "mesh.h"
#include "log.h"

cfg       config;
uint32_t  gathered_smpls = 0;
vec3      bg_col = { 0.0f, 0.0f, 0.0f };

view      curr_view;
cam       curr_cam;

mesh      *curr_mesh;

bool      orbit_cam = false;

mesh *init_scene(size_t tri_cnt)
{
  vec3 vertices[tri_cnt * 3];

  for(size_t i=0; i<tri_cnt * 3; i++)
    vertices[i] = vec3_rand();

  return mesh_create(vertices, tri_cnt * 3);
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

  curr_mesh = init_scene(16);

  gpu_write_buf(GLOB, GLOB_BUF_OFS_CFG, &config, sizeof(cfg));

  update_cam_view();
}

__attribute__((visibility("default")))
void update(float time)
{
  float frame[8] = { randf(), config.spp / (float)(gathered_smpls + config.spp),
    time, 0.0f, bg_col.x, bg_col.y, bg_col.z, 0.0f };
  gpu_write_buf(GLOB, GLOB_BUF_OFS_FRAME, frame, 8 * sizeof(float));

  gathered_smpls += config.spp;
}

__attribute__((visibility("default")))
void release(void)
{
  mesh_release(curr_mesh);
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
