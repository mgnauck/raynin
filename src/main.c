#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "base/log.h"
#include "base/math.h"
#include "base/stdlib.h"
#include "imex/import.h"
#include "rend/bvh.h"
#include "rend/renderer.h"
#include "scene/cam.h"
#include "scene/inst.h"
#include "scene/mesh.h"
#include "scene/mtl.h"
#include "scene/scene.h"
#include "util/vec3.h"

extern void set_ltri_cnt(uint32_t n);
extern void toggle_converge();

#define MAX_SCENES 10

scene     *scenes = NULL;
uint8_t   scene_cnt = 0;
uint8_t   active_scene_id = 0;
scene     *active_scene = NULL;
uint16_t  active_cam_id = 0;

__attribute__((visibility("default")))
void key_down(unsigned char key, float move_vel)
{
  vec3 gmin = active_scene->tlas_nodes[0].min;
  vec3 gmax = active_scene->tlas_nodes[0].max;

  float speed = vec3_max_comp(vec3_scale(vec3_sub(gmax, gmin), 0.2f));

  cam *cam = scene_get_active_cam(active_scene);

  switch(key) {
    case 'a':
      cam->eye = vec3_add(cam->eye, vec3_scale(cam->right, -speed * move_vel));
      break;
    case 'd':
      cam->eye = vec3_add(cam->eye, vec3_scale(cam->right, speed * move_vel));
      break;
    case 'w':
      cam->eye = vec3_add(cam->eye, vec3_scale(cam->fwd, -speed * move_vel));
      break;
    case 's':
      cam->eye = vec3_add(cam->eye, vec3_scale(cam->fwd, speed * move_vel));
      break;
    case 'i':
      cam->foc_dist += 0.1f;
      break;
    case 'k':
      cam->foc_dist = max(cam->foc_dist - 0.1f, 0.1f);
      break;
    case 'j':
      cam->foc_angle = max(cam->foc_angle - 0.1f, 0.1f);
      break;
    case 'l':
      cam->foc_angle += 0.1f;
      break;
    case 'v':
      active_scene_id =
          (active_scene_id > 0) ? active_scene_id - 1 : scene_cnt - 1;
      active_scene = &scenes[active_scene_id];
      scene_set_dirty(active_scene, RT_CFG | RT_CAM | RT_MTL | RT_TRI |
                                        RT_LTRI | RT_INST | RT_BLAS);
      break;
    case 'b':
      active_scene_id = (active_scene_id + 1) % scene_cnt;
      active_scene = &scenes[active_scene_id];
      scene_set_dirty(active_scene, RT_CFG | RT_CAM | RT_MTL | RT_TRI |
                                        RT_LTRI | RT_INST | RT_BLAS);
      break;
    case 'n':
      active_cam_id =
          active_cam_id > 0 ? active_cam_id - 1 : active_scene->cam_cnt - 1;
      scene_set_active_cam(active_scene, active_cam_id);
      break;
    case 'm':
      active_cam_id = (active_cam_id + 1) % active_scene->cam_cnt;
      scene_set_active_cam(active_scene, active_cam_id);
      break;
    case 'c':
      toggle_converge();
      break;
  }

  scene_set_dirty(active_scene, RT_CAM);
}

__attribute__((visibility("default")))
void mouse_move(int32_t dx, int32_t dy, float look_vel)
{
  cam *cam = scene_get_active_cam(active_scene);

  float theta =
      min(max(acosf(-cam->fwd.y) + (float)dy * look_vel, 0.05f), 0.95f * PI);
  float phi = fmodf(atan2f(-cam->fwd.z, cam->fwd.x) + PI - (float)dx * look_vel,
                    2.0f * PI);

  cam_set_dir(cam, vec3_spherical(theta, phi));

  scene_set_dirty(active_scene, RT_CAM);
}

__attribute__((visibility("default")))
uint8_t load_scene_gltf(const char *gltf, size_t gltf_sz,
    const unsigned char *bin, size_t bin_sz)
{
  if(scenes == NULL)
    scenes = malloc(MAX_SCENES * sizeof(*scenes));

  scene *s = scenes + scene_cnt;

  if(import_gltf(s, gltf, gltf_sz, bin, bin_sz) > 0)
    return 1;

  scene_cnt++;

  logc("Imported gltf scene with: %i tris, %i ltris, %i mtls, %i insts",
       s->max_tri_cnt, s->max_ltri_cnt, s->max_mtl_cnt, s->max_inst_cnt);

  return 0;
}

__attribute__((visibility("default")))
void init()
{
  // Find max resource sizes among the different scenes
  uint32_t max_tri_cnt = 0;
  uint32_t max_ltri_cnt = 0;
  uint16_t max_mtl_cnt = 0;
  uint16_t max_inst_cnt = 0;
  for(uint8_t i=0; i<scene_cnt; i++) {
    scene *s = &scenes[i];
    max_tri_cnt = max(max_tri_cnt, s->max_tri_cnt);
    max_ltri_cnt = max(max_ltri_cnt, s->max_ltri_cnt);
    max_mtl_cnt = max(max_mtl_cnt, s->max_mtl_cnt);
    max_inst_cnt = max(max_inst_cnt, s->max_inst_cnt);
  }

  // Allocate max resources
  renderer_gpu_alloc(max_tri_cnt, max_ltri_cnt, max_mtl_cnt, max_inst_cnt);

  logc(
      "Allocated gpu resources for max %i tris, %i ltris, %i mtls and %i insts",
      max_tri_cnt, max_ltri_cnt, max_mtl_cnt, max_inst_cnt);

  // Set active scene
  active_scene = &scenes[active_scene_id];

  scene_set_dirty(active_scene, RT_CFG | RT_CAM | RT_MTL | RT_TRI | RT_LTRI |
      RT_INST | RT_BLAS);
}

__attribute__((visibility("default")))
void update(float time, bool converge)
{
  renderer_update(active_scene, converge);
  set_ltri_cnt(active_scene->ltri_cnt);
}

__attribute__((visibility("default")))
void release()
{
  for(uint8_t i=0; i<scene_cnt; i++)
    scene_release(&scenes[i]);
  free(scenes);
}
