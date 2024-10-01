#include <stdbool.h>
#include <stdint.h>
#include "base/math.h"
#include "base/log.h"
#include "base/walloc.h"
#include "imex/escene.h"
#include "imex/export.h"
#include "imex/import.h"
#include "rend/bvh.h"
#include "rend/renderer.h"
#include "scene/cam.h"
#include "scene/inst.h"
#include "scene/mesh.h"
#include "scene/mtl.h"
#include "scene/scene.h"
#include "sync/sync.h"
#include "sync/sync_types.h"
#include "util/vec3.h"

//#define NO_CONTROL
//#define TINY_BUILD
//#define LOAD_EMBEDDED_DATA

#ifdef LOAD_EMBEDDED_DATA
#include "scene_data.h"
#endif

// Import from JS
extern void set_ltri_cnt(uint32_t n);

scene     *scenes = NULL;
uint8_t   scene_cnt = 0;
uint8_t   active_scene_id = 0;
scene     *active_scene = NULL;
uint16_t  active_cam_id = 0;
track     intro;

#ifndef NO_CONTROL
extern void toggle_converge();
extern void toggle_filter();
extern void toggle_reprojection();
extern void save_binary(const void *ptr, uint32_t sz);
#endif

#ifndef TINY_BUILD
// Export scenes
#define MAX_SCENES 20
escene    escenes[MAX_SCENES];
uint8_t   escene_cnt = 0;
#endif

#ifndef NO_CONTROL
__attribute__((visibility("default")))
void key_down(unsigned char key, float move_vel)
{
  vec3 gmin = vec3_min(active_scene->tlas_nodes[0].lmin, active_scene->tlas_nodes[0].rmin);
  vec3 gmax = vec3_max(active_scene->tlas_nodes[0].lmax, active_scene->tlas_nodes[0].rmax);

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
    case 'u':
      active_scene->bg_col = vec3_sub((vec3){1.0f, 1.0f, 1.0f }, active_scene->bg_col);
      scene_set_dirty(active_scene, RT_CFG);
      break;
    case 'v':
      active_scene_id = (active_scene_id > 0 ) ? active_scene_id - 1 : scene_cnt - 1;
      active_scene = &scenes[active_scene_id];
      scene_set_dirty(active_scene, RT_CFG | RT_CAM | RT_MTL | RT_TRI | RT_LTRI | RT_INST | RT_BLAS);
      break;
    case 'b':
      active_scene_id = (active_scene_id + 1) % scene_cnt;
      active_scene = &scenes[active_scene_id];
      scene_set_dirty(active_scene, RT_CFG | RT_CAM | RT_MTL | RT_TRI | RT_LTRI | RT_INST | RT_BLAS);
      break;
    case 'n':
      active_cam_id = active_cam_id > 0 ? active_cam_id - 1 : active_scene->cam_cnt - 1;
      scene_set_active_cam(active_scene, active_cam_id);
      logc("Setting cam %i of %i cams active", active_cam_id, active_scene->cam_cnt);
      break;
    case 'm':
      active_cam_id = (active_cam_id + 1) % active_scene->cam_cnt;
      scene_set_active_cam(active_scene, active_cam_id);
      logc("Setting cam %i of %i cams active", active_cam_id, active_scene->cam_cnt);
      break;
   case ' ':
      toggle_converge();
      break;
   case 'f':
      toggle_filter();
      break;
   case 'g':
      toggle_reprojection();
      break;
  }

  scene_set_dirty(active_scene, RT_CAM);
}

__attribute__((visibility("default")))
void mouse_move(int32_t dx, int32_t dy, float look_vel)
{
  cam *cam = scene_get_active_cam(active_scene);

  float theta = min(max(acosf(-cam->fwd.y) + (float)dy * look_vel, 0.05f), 0.95f * PI);
  float phi = fmodf(atan2f(-cam->fwd.z, cam->fwd.x) + PI - (float)dx * look_vel, 2.0f * PI);

  cam_set_dir(cam, vec3_spherical(theta, phi));

  scene_set_dirty(active_scene, RT_CAM);
}
#endif

#ifndef TINY_BUILD
__attribute__((visibility("default")))
uint8_t load_scene_gltf(const char *gltf, size_t gltf_sz, const unsigned char *bin, size_t bin_sz, bool prepare_for_export)
{
  uint8_t ret = 0;

  if(scenes == NULL)
    scenes = malloc(MAX_SCENES * sizeof(*scenes));

  // Scene to load
  scene *s = scenes + scene_cnt;

  // Import scene from given GLTF for rendering
  if(import_gltf(s, gltf, gltf_sz, bin, bin_sz) == 0)
    scene_cnt++;
  else
    return 1;

  logc("Imported gltf scene with: %i tris, %i ltris, %i mtls, %i insts",
      s->max_tri_cnt, s->max_ltri_cnt, s->max_mtl_cnt, s->max_inst_cnt);

  // Import the scene again for export
  if(prepare_for_export && escene_cnt < MAX_SCENES) {
    if(import_gltf_ex(&escenes[escene_cnt], gltf, gltf_sz, bin, bin_sz) == 0) {
      escene_cnt++;
      logc("Created export scene from gltf scene");
    } else
      logc("Failed to import gltf scene for export");
  }

  return 0;
}

__attribute__((visibility("default")))
uint8_t export_scenes()
{
  if(escene_cnt > 0) {
    uint8_t *bin;
    size_t bin_sz;
    if(export_bin(&bin, &bin_sz, escenes, escene_cnt) == 0) {
      save_binary(bin, bin_sz);
      free(bin);
      for(uint8_t i=0; i<escene_cnt; i++)
        escene_release(&escenes[i]);
      escene_cnt = 0;
      return 0;
    }
  }

  return 1;
}
#endif

__attribute__((visibility("default")))
void add_event(uint16_t row, uint8_t id, float value, uint8_t blend_type)
{
  // Event specified from JS
  sync_add_event(&intro, row, id, value, blend_type);
}

__attribute__((visibility("default")))
uint8_t load_scenes_bin(uint8_t *bin)
{
#ifndef LOAD_EMBEDDED_DATA
  return import_bin(&scenes, &scene_cnt, bin);
#else
  return 0;
#endif
}

__attribute__((visibility("default")))
void init(uint16_t bpm, uint8_t rows_per_beat, uint16_t event_cnt)
{
#ifdef LOAD_EMBEDDED_DATA
  // Load our scenes
  import_bin(&scenes, &scene_cnt, scene_data);
#endif

  // Init the sync track
  sync_init_track(&intro, event_cnt, bpm, rows_per_beat);

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

  // Allocated space for scene resources on GPU
  renderer_gpu_alloc(max_tri_cnt, max_ltri_cnt, max_mtl_cnt, max_inst_cnt);

#ifndef TINY_BUILD
  logc("Allocated gpu resources for max %i tris, %i ltris, %i mtls and %i insts",
      max_tri_cnt, max_ltri_cnt, max_mtl_cnt, max_inst_cnt);
#endif

  // Initialize active scene
  active_scene = &scenes[active_scene_id];
}

void process_events(const track *track, float time)
{
  if(track->event_cnt == 0)
    return;

  // Set the active scene (SCN_ID)
  active_scene_id = (uint8_t)sync_get_value(track, SCN_ID, time);
  if(active_scene_id >= 0 && active_scene_id < scene_cnt)
    active_scene = &scenes[active_scene_id];
  else {
    logc("#### ERROR Sync track requested unavailable scene");
    active_scene_id = 0;
  }

  // Set active cam (CAM_ID)
  uint16_t cam_id = (uint16_t)sync_get_value(track, CAM_ID, time);
  if(cam_id >= 0 && cam_id < active_scene->cam_cnt) {
    scene_set_active_cam(active_scene, cam_id);
  } else
    logc("#### ERROR Sync track requested unavailable camera");

  /*float eye_z = sync_get_value(track, CAM_EYE_Z, time);
  if(eye_z != BROKEN_EVENT) {
    cam *cam = scene_get_active_cam(active_scene);
    vec3 tgt = vec3_add(cam->eye, cam->fwd);
    cam_set(cam, vec3_add(cam->eye, (vec3){ 0.0, 0.0, eye_z }), tgt);
  }*/
}

__attribute__((visibility("default")))
void update(float time, bool converge)
{
  process_events(&intro, time);
  renderer_update(active_scene, converge);
  set_ltri_cnt(active_scene->ltri_cnt);
}

#ifndef TINY_BUILD
__attribute__((visibility("default")))
void release()
{
  sync_release_track(&intro);

  for(uint8_t i=0; i<scene_cnt; i++)
    scene_release(&scenes[i]);
  free(scenes);
}
#endif
