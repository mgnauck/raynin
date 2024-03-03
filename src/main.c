#include <stdbool.h>
#include <stdint.h>
#include "mutil.h"
#include "vec3.h"
#include "mesh.h"
#include "mtl.h"
#include "scene.h"
#include "renderer.h"
#include "log.h"

#include "data/teapot.h"
#include "data/dragon.h"
#include "data/icosahedron.h"

#define RIOW_SIZE   22
#define TRI_CNT     1280 + 2
#define MESH_CNT    2
#define INST_CNT    (RIOW_SIZE * RIOW_SIZE + 4)
#define MTL_CNT     INST_CNT

scene       scn;
render_data *rd;

bool        orbit_cam = false;
bool        paused = false;

  __attribute__((visibility("default")))
void key_down(unsigned char key)
{
#define MOVE_VEL 0.1f

  switch(key) {
    case 'a':
      scn.cam.eye = vec3_add(scn.cam.eye, vec3_scale(scn.cam.right, -MOVE_VEL));
      break;
    case 'd':
      scn.cam.eye = vec3_add(scn.cam.eye, vec3_scale(scn.cam.right, MOVE_VEL));
      break;
    case 'w':
      scn.cam.eye = vec3_add(scn.cam.eye, vec3_scale(scn.cam.fwd, -MOVE_VEL));
     break;
    case 's':
      scn.cam.eye = vec3_add(scn.cam.eye, vec3_scale(scn.cam.fwd, MOVE_VEL));
      break;
    case 'i':
      scn.cam.foc_dist += 0.1f;
      break;
    case 'k':
      scn.cam.foc_dist = max(scn.cam.foc_dist - 0.1f, 0.1f);
      break;
    case 'j':
      scn.cam.foc_angle = max(scn.cam.foc_angle - 0.1f, 0.1f);
      break;
    case 'l':
      scn.cam.foc_angle += 0.1f;
      break;
    case 'o':
      orbit_cam = !orbit_cam;
      break;
    case 'p':
      paused = !paused;
      break;
    case 'r':
      // TODO Call JS to reload shader
      break;
  }

  renderer_set_dirty(rd, CAM_VIEW);
}

__attribute__((visibility("default")))
void mouse_move(int32_t dx, int32_t dy)
{
#define LOOK_VEL 0.015f
  
  float theta = min(max(acosf(-scn.cam.fwd.y) + (float)dy * LOOK_VEL, 0.01f), 0.99f * PI);
  float phi = fmodf(atan2f(-scn.cam.fwd.z, scn.cam.fwd.x) + PI - (float)dx * LOOK_VEL, 2.0f * PI);
  
  cam_set_dir(&scn.cam, vec3_spherical(theta, phi));
  
  renderer_set_dirty(rd, CAM_VIEW);
}

void init_scene_riow(scene *s)
{
  scene_init(s, MESH_CNT, MTL_CNT, INST_CNT);

  s->cam = (cam){ .vert_fov = 20.0f, .foc_dist = 10.0f, .foc_angle = 0.6f };
  cam_set(&s->cam, (vec3){ 13.0f, 2.0f, 3.0f }, (vec3){ 0.0f, 0.0f, 0.0f });

  // Meshes
  scene_add_uvsphere(s, 1.0f, 20, 20, false);
  scene_add_quad(s, (vec3){ 0.0f, 0.f, 0.0f }, (vec3){ 0.0f, 1.0f, 0.0f }, 40.0f, 40.0f);
  scene_build_bvhs(s);

  // Floor instance
  mat4 translation;
  mat4_trans(translation, (vec3){ 0.0f, 0.0f, 0.0f });
  uint32_t mtl_id = scene_add_mtl(s, &(mtl){ .color = { 0.5f, 0.5f, 0.5f }, .value = 0.0f });
  scene_add_inst(s, 1, mtl_id, translation); 

  // Sphere instances
  mat4_trans(translation, (vec3){ 4.0f, 1.0f, 0.0f });
  mtl_id = scene_add_mtl(s, &(mtl){ .color = { 0.7f, 0.6f, 0.5f }, .value = 0.001f });
  scene_add_inst(s, 0, mtl_id, translation);

  mat4_trans(translation, (vec3){ 0.0f, 1.0f, 0.0f });
  mtl_id = scene_add_mtl(s, &(mtl){ .color = { 1.0f, 1.0f, 1.0f }, .value = 1.5f });
  scene_add_inst(s, 0, mtl_id, translation);

  mat4_trans(translation, (vec3){ -4.0f, 1.0f, 0.0f });
  mtl_id = scene_add_mtl(s, &(mtl){ .color = { 0.4f, 0.2f, 0.1f }, .value = 0.0f });
  scene_add_inst(s, 0, mtl_id, translation);

  mat4 scale;
  mat4_scale(scale, 0.2f);
  
  for(int a=-RIOW_SIZE/2; a<RIOW_SIZE/2; a++) {
    for(int b=-RIOW_SIZE/2; b<RIOW_SIZE/2; b++) {
      float mtl_p = pcg_randf();
      vec3 center = { (float)a + 0.9f * pcg_randf(), 0.2f, (float)b + 0.9f * pcg_randf() };
      if(vec3_len(vec3_add(center, (vec3){ -4.0f, -0.2f, 0.0f })) > 0.9f) {
        if(mtl_p < 0.8f)
          mtl_id = scene_add_mtl(s, &(mtl){ .color = vec3_mul(vec3_rand(), vec3_rand()), .value = 0.0f });
        else if(mtl_p < 0.95f)
          mtl_id = scene_add_mtl(s, &(mtl){ .color = vec3_rand_rng(0.5f, 1.0f), .value = pcg_randf_rng(0.001f, 0.5f) });
        else
          mtl_id = scene_add_mtl(s, &(mtl){ .color = (vec3){ 1.0f, 1.0f, 1.0f }, .value = 1.5f });
        
        mat4 transform;
        mat4_trans(translation, center);
        mat4_mul(transform, translation, scale);
        scene_add_inst(s, 0, mtl_id, transform);
      }
    }
  }
}

__attribute__((visibility("default")))
void init(uint32_t width, uint32_t height)
{
  pcg_srand(42u, 303u);

  init_scene_riow(&scn);

  rd = renderer_init(&scn, width, height, 2);
  
  renderer_push_static_res(rd);
  renderer_set_bg_col(rd, (vec3){ 0.7f, 0.8f, 1.0f });
}

void update_scene(float time)
{
  // Update camera
  if(orbit_cam) {
    float s = 0.5f;
    float r = 16.0f;
    float h = 2.0f;
    vec3 pos = (vec3){ r * sinf(time * s * s), 0.2f + h + h * sinf(time * s * 0.7f), r * cosf(time * s) };
    cam_set(&scn.cam, pos, vec3_neg(pos));
    renderer_set_dirty(rd, CAM_VIEW);
  }

  // Update instances
}

__attribute__((visibility("default")))
void update(float time)
{
  update_scene(time);
  renderer_update(rd, time);
}

__attribute__((visibility("default")))
void release()
{
  renderer_release(rd);
  scene_release(&scn);
}
