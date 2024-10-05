#include <stdbool.h>
#include <stdint.h>
#include "base/math.h"
#include "base/log.h"
#include "base/walloc.h"
#include "imex/escene.h"
#include "imex/export.h"
#include "imex/import.h"
#include "rend/bvh.h"
#include "rend/postparams.h"
#include "rend/renderer.h"
#include "scene/cam.h"
#include "scene/inst.h"
#include "scene/mesh.h"
#include "scene/mtl.h"
#include "scene/scene.h"
#include "sync/sync.h"
#include "sync/sync_types.h"
#include "util/vec3.h"

// #define NO_CONTROL
// #define TINY_BUILD
// #define LOAD_EMBEDDED_DATA

#ifdef LOAD_EMBEDDED_DATA
#include "scene_data.h"
#endif

// Import from JS
extern void set_ltri_cnt(uint32_t n);

scene *scenes = NULL;
uint8_t scene_cnt = 0;
uint8_t active_scene_id = 0;
scene *active_scene = NULL;
uint16_t active_cam_id = 0;
post_params post;
bool active_post = false;
track intro;

// Animations
#define ANIMATION_MAX_TRANSFORMS 75
float active_scene_time = 0.f;
bool active_scene_changed = false;
bool active_cam_changed = false;
mat4 animation_transforms[ANIMATION_MAX_TRANSFORMS];

#ifndef NO_CONTROL
extern void toggle_edit();
extern void toggle_converge();
extern void toggle_filter();
extern void toggle_reprojection();
extern void save_binary(const void *ptr, uint32_t sz);
#endif

#ifndef TINY_BUILD
// Export scenes
#define MAX_SCENES 20
escene escenes[MAX_SCENES];
uint8_t escene_cnt = 0;
#endif

#ifndef NO_CONTROL
__attribute__((visibility("default"))) void key_down(unsigned char key, float move_vel)
{
  vec3 gmin = vec3_min(active_scene->tlas_nodes[0].lmin, active_scene->tlas_nodes[0].rmin);
  vec3 gmax = vec3_max(active_scene->tlas_nodes[0].lmax, active_scene->tlas_nodes[0].rmax);

  float speed = vec3_max_comp(vec3_scale(vec3_sub(gmax, gmin), 0.2f));

  cam *cam = scene_get_active_cam(active_scene);

  switch (key)
  {
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
    active_scene->bg_col = vec3_sub((vec3){1.0f, 1.0f, 1.0f}, active_scene->bg_col);
    scene_set_dirty(active_scene, RT_CFG);
    break;
  case 'v':
    active_scene_id = (active_scene_id > 0) ? active_scene_id - 1 : scene_cnt - 1;
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
  case 'c':
    toggle_converge();
    break;
  case 'p':
    toggle_edit();
    break;
  case ' ':
    logc("---> scene:");
    logc("   r, %4i, %8i, %2i, // SCN_ID", SCN_ID, active_scene_id, BT_STEP);
    logc("<----");
    logc("---> camera:");
    logc("   r, %4i, %8.3f, %2i, // CAM_POS_X\n   r, %4i, %8.3f, %2i, // CAM_POS_Y\n   r, %4i, %8.3f, %2i, // CAM_POS_Z\n"
         "   r, %4i, %8.3f, %2i, // CAM_DIR_X\n   r, %4i, %8.3f, %2i, // CAM_DIR_Y\n   r, %4i, %8.3f, %2i, // CAM_DIR_Z\n"
         "   r, %4i, %8.3f, %2i, // CAM_FOV",
         CAM_POS_X, cam->eye.x, BT_STEP, CAM_POS_Y, cam->eye.y, BT_STEP, CAM_POS_Z, cam->eye.z, BT_STEP,
         CAM_DIR_X, cam->fwd.x, BT_STEP, CAM_DIR_Y, cam->fwd.y, BT_STEP, CAM_DIR_Z, cam->fwd.z, BT_STEP,
         CAM_FOV, cam->vert_fov, BT_STEP);
    logc("<----");
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

__attribute__((visibility("default"))) void mouse_move(int32_t dx, int32_t dy, float look_vel)
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
uint8_t
load_scene_gltf(const char *gltf, size_t gltf_sz, const unsigned char *bin, size_t bin_sz, bool prepare_for_export)
{
  uint8_t ret = 0;

  if (scenes == NULL)
    scenes = malloc(MAX_SCENES * sizeof(*scenes));

  // Scene to load
  scene *s = scenes + scene_cnt;

#if true
  logc("[!!11] Scene id: %d", scene_cnt);
#endif

  // Import scene from given GLTF for rendering
  if (import_gltf(s, gltf, gltf_sz, bin, bin_sz) == 0)
    scene_cnt++;
  else
    return 1;

  logc("Imported gltf scene with: %i tris, %i ltris, %i mtls, %i insts",
       s->max_tri_cnt, s->max_ltri_cnt, s->max_mtl_cnt, s->max_inst_cnt);

  // Import the scene again for export
  if (prepare_for_export && escene_cnt < MAX_SCENES)
  {
    if (import_gltf_ex(&escenes[escene_cnt], gltf, gltf_sz, bin, bin_sz) == 0)
    {
      escene_cnt++;
      logc("Created export scene from gltf scene");
    }
    else
      logc("Failed to import gltf scene for export");
  }

  return 0;
}

__attribute__((visibility("default")))
uint8_t
export_scenes()
{
  if (escene_cnt > 0)
  {
    uint8_t *bin;
    size_t bin_sz;
    if (export_bin(&bin, &bin_sz, escenes, escene_cnt) == 0)
    {
      save_binary(bin, bin_sz);
      free(bin);
      for (uint8_t i = 0; i < escene_cnt; i++)
        escene_release(&escenes[i]);
      escene_cnt = 0;
      return 0;
    }
  }

  return 1;
}
#endif

__attribute__((visibility("default"))) void add_event(uint16_t row, uint8_t id, float value, uint8_t blend_type)
{
  sync_add_event(&intro, row, id, value, blend_type);
}

__attribute__((visibility("default")))
uint8_t
load_scenes_bin(uint8_t *bin)
{
#ifndef LOAD_EMBEDDED_DATA
  return import_bin(&scenes, &scene_cnt, bin);
#else
  return 0;
#endif
}

__attribute__((visibility("default"))) void init(uint16_t bpm, uint8_t rows_per_beat, uint16_t event_cnt)
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

  for (uint8_t i = 0; i < scene_cnt; i++)
  {
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

__attribute__((visibility("default"))) void finalize_resources()
{
  sync_index_track(&intro);

  for (uint8_t i = 0; i < scene_cnt; i++)
  {
    renderer_update(&scenes[i], NULL, false);
    scene_set_dirty(active_scene, RT_CFG | RT_CAM | RT_MTL | RT_TRI | RT_LTRI | RT_INST | RT_BLAS);
  }
}

void process_events(track *track, float time)
{
  // Set the active scene (SCN_ID)
  float fid = sync_event_get_value(track, SCN_ID, time);
  if (fid != EVENT_INACTIVE)
  {
    uint8_t id = (uint8_t)fid;
    if (id >= 0 && id < scene_cnt)
    {
      if (id != active_scene_id)
      {
        active_scene_id = id;
        active_scene = &scenes[active_scene_id];
        scene_set_dirty(active_scene, RT_CFG | RT_CAM | RT_MTL | RT_TRI | RT_LTRI | RT_INST | RT_BLAS);
        active_scene_time = time;
        active_scene_changed = true;
      }
    }
    else
      logc("#### ERROR Sync track requested unavailable scene");
  }

  cam *cam = scene_get_active_cam(active_scene);

  // Camera pos/dir
  float px = sync_event_get_value(track, CAM_POS_X, time);
  if (px != EVENT_INACTIVE)
  {
    float py = sync_event_get_value(track, CAM_POS_Y, time);
    float pz = sync_event_get_value(track, CAM_POS_Z, time);
    float dx = sync_event_get_value(track, CAM_DIR_X, time);
    float dy = sync_event_get_value(track, CAM_DIR_Y, time);
    float dz = sync_event_get_value(track, CAM_DIR_Z, time);
    cam_set(cam, (vec3){px, py, pz}, (vec3){dx, dy, dz});
    scene_set_dirty(active_scene, RT_CAM);
  }

  // Camera fov
  float fov = sync_event_get_value(track, CAM_FOV, time);
  if (fov != EVENT_INACTIVE)
  {
    cam->vert_fov = fov;
    scene_set_dirty(active_scene, RT_CAM);
  }

  float val = sync_event_get_value(track, FADE_VAL, time);
  if (val != EVENT_INACTIVE)
  {
    post.fade_col.x = sync_event_get_value(track, FADE_COL_R, time);
    post.fade_col.y = sync_event_get_value(track, FADE_COL_G, time);
    post.fade_col.z = sync_event_get_value(track, FADE_COL_B, time);
    post.fade_val = val;
    active_post = true;
  }
  else
    active_post = false;
}

static uint8_t yellow_sphere_offsets[14] =
    {28 /* center sphere */, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27};

// TODO: 'Sphere.009' does not exist as a node
static uint8_t marble_sphere_offsets[43] =
    {37, 38, 0, 1, 39, 2, 40, 3, 4, 5,
     6, 41, 7, 8, 42, 9, 10, 43, 44, 11,
     12, 45, 13, 14, 15, 46, 16, 17, 47, 18,
     19, 48, 20, 21, 49, 22, 23, 24, 50, 51,
     25, 52, 26};

// immer 4 sagen

static uint8_t good7_sphere_offsets[58] =
    {57 /* center sphere */, 8, 9, 10, 11, 12, 13, 14, 15, 16,
     17, 18, 19, 20, 21, 22, 23, 24, 25, 26,
     27, 28, 29, 30, 31, 32, 33, 34, 35, 36,
     37, 38, 39, 40, 41, 42, 43, 44, 45, 46,
     47, 48, 49, 50, 51, 52, 53, 54, 55, 56,
     70, 71, 72, 73, 74, 75, 76, 77};

/*
static uint8_t disco_offsets[] =
    {37, 38, 0, 1, 39, 2, 40, 3, 4, 5,
     6, 41, 7, 8, 42, 9, 10, 43, 44, 11,
     12, 45, 13, 14, 15, 46, 16, 17, 47, 18,
     19, 48, 20, 21, 49, 22, 23, 24, 50, 51,
     25, 52, 26};
*/

void handle_animations(track *track, float time)
{
  // time is relative to when the scene became active
  float scene_time = time - active_scene_time;
  scene *scene = active_scene;

  // get trigger values
  float trigger0_val = sync_event_get_value(track, TRIGGER_0, time);

// scenes
#define SCENE_YELLOW_SUBMARINE 1
#define SCENE_MARBLES 4
#define SCENE_GOOD7 6
#define SCENE_DISCO 9

  if (active_scene_changed)
  {
    logc("active_scene_changed to %d", active_scene_id);
  }

  // SCENE_YELLOW_SUBMARINE
  if (active_scene_id == SCENE_YELLOW_SUBMARINE)
  {
    uint8_t yellow_sphere_offsets_cnt = sizeof(yellow_sphere_offsets) / sizeof(yellow_sphere_offsets[0]);

    // init
    if (active_scene_changed)
    {
      for (uint8_t i = 0; i < yellow_sphere_offsets_cnt; i++)
      {
        uint8_t offset = yellow_sphere_offsets[i];
        inst_info *inst_info = &scene->inst_info[offset];
        mat4_copy(animation_transforms[i], inst_info->transform);
      }
    }

    // TODO grab center ob sphere offset 0
    mat4 center_sphere_mat;
    mat4_copy(center_sphere_mat, animation_transforms[0]);
    vec3 center_sphere_pos = mat4_get_trans(center_sphere_mat);

    if (active_scene_changed)
    {
      vec3_logc("center_sphere_pos: ", center_sphere_pos);
    }

    // TODO translate all other spheres by this offset and rotate them
    for (uint8_t i = 1; i < yellow_sphere_offsets_cnt; i++)
    {
      uint8_t offset = yellow_sphere_offsets[i];

      mat4 sphere_mat;
      mat4_copy(sphere_mat, animation_transforms[i]);
      vec3 sphere_pos = mat4_get_trans(sphere_mat);

      // sub center sphere translation from sphere pos
      sphere_pos = vec3_sub(sphere_pos, center_sphere_pos);

      sphere_mat[3] = sphere_pos.x;
      sphere_mat[7] = sphere_pos.y;
      sphere_mat[11] = sphere_pos.z;

      float rot = time;
      mat4 sphere_rot_x;
      mat4 sphere_rot_y;
      mat4_rot_x(sphere_rot_x, rot);
      mat4_rot_y(sphere_rot_y, rot);
      mat4_mul(sphere_mat, sphere_rot_x, sphere_mat);
      mat4_mul(sphere_mat, sphere_rot_y, sphere_mat);

      sphere_mat[3] += center_sphere_pos.x;
      sphere_mat[7] += center_sphere_pos.y;
      sphere_mat[11] += center_sphere_pos.z;

      if (active_scene_changed)
      {
        vec3_logc("sphere_pos: ", sphere_pos);
      }

      scene_upd_inst_trans(scene, offset, sphere_mat);
    }
  }
  else if (active_scene_id == SCENE_MARBLES)
  {
    uint8_t marble_sphere_offset_cnt = sizeof(marble_sphere_offsets) / sizeof(marble_sphere_offsets[0]);

    // init
    if (active_scene_changed)
    {
      for (uint8_t i = 0; i < marble_sphere_offset_cnt; i++)
      {
        uint8_t offset = marble_sphere_offsets[i];
        inst_info *inst_info = &scene->inst_info[offset];
        mat4_copy(animation_transforms[i], inst_info->transform);
      }
    }

    // animate only if trigger0 > 0
    if (trigger0_val > 0)
    {
      for (uint8_t i = 0; i < marble_sphere_offset_cnt; i++)
      {
        uint8_t offset = marble_sphere_offsets[i];

        mat4 transform;
        mat4_copy(transform, animation_transforms[i]);

        float scale = 1.f + sinf(i * 0.9f + 5.f * time) * 0.2f;

        // add trigger0 to control scale
        //scale *= + trigger0_val;

        vec3 scale_vec = {scale, scale, scale};
        mat4 scale_mat;
        mat4_scale(scale_mat, scale_vec);
        mat4_mul(transform, transform, scale_mat);

        scene_upd_inst_trans(scene, offset, transform);
      }
    }
  }
  else
    // SCENE_GOOD7
    if (active_scene_id == SCENE_GOOD7)
    {
#define OFFSETS good7_sphere_offsets
      uint8_t *offsets = OFFSETS;
      uint8_t offsets_cnt = sizeof(OFFSETS) / sizeof(OFFSETS[0]);

      // init
      if (active_scene_changed)
      {
        for (uint8_t i = 0; i < offsets_cnt; i++)
        {
          uint8_t offset = offsets[i];
          inst_info *inst_info = &scene->inst_info[offset];
          mat4_copy(animation_transforms[i], inst_info->transform);
        }
      }

      // grab center ob sphere offset 0
      mat4 center_sphere_mat;
      mat4_copy(center_sphere_mat, animation_transforms[0]);
      vec3 center_sphere_pos = mat4_get_trans(center_sphere_mat);

      // translate all other spheres by this offset and rotate them
      for (uint8_t i = 1; i < offsets_cnt; i++)
      {
        uint8_t offset = offsets[i];

        mat4 sphere_mat;
        mat4_copy(sphere_mat, animation_transforms[i]);
        vec3 sphere_pos = mat4_get_trans(sphere_mat);

        // sub center sphere translation from sphere pos
        sphere_pos = vec3_sub(sphere_pos, center_sphere_pos);

        sphere_mat[3] = sphere_pos.x;
        sphere_mat[7] = sphere_pos.y;
        sphere_mat[11] = sphere_pos.z;

        // mul by trigger0 to switch direction
        float rot = time * trigger0_val;
        mat4 sphere_rot_y;
        mat4_rot_y(sphere_rot_y, rot);
        mat4_mul(sphere_mat, sphere_rot_y, sphere_mat);

        sphere_mat[3] += center_sphere_pos.x;
        sphere_mat[7] += center_sphere_pos.y;
        sphere_mat[11] += center_sphere_pos.z;

        scene_upd_inst_trans(scene, offset, sphere_mat);
      }
    }
    else if (active_scene_id == SCENE_MARBLES)
    {
      uint8_t marble_sphere_offset_cnt = sizeof(marble_sphere_offsets) / sizeof(marble_sphere_offsets[0]);

      // init
      if (active_scene_changed)
      {
        for (uint8_t i = 0; i < marble_sphere_offset_cnt; i++)
        {
          uint8_t offset = marble_sphere_offsets[i];
          inst_info *inst_info = &scene->inst_info[offset];
          mat4_copy(animation_transforms[i], inst_info->transform);
        }
      }

      // bool animate = scene_time > 4.f;
      if (true)
      {
        for (uint8_t i = 0; i < marble_sphere_offset_cnt; i++)
        {
          uint8_t offset = marble_sphere_offsets[i];

          mat4 transform;
          mat4_copy(transform, animation_transforms[i]);

          float scale = 1.f + sinf(i * 0.9f + 5.f * time) * 0.2f;
          vec3 scale_vec = {scale, scale, scale};
          mat4 scale_mat;
          mat4_scale(scale_mat, scale_vec);
          mat4_mul(transform, transform, scale_mat);

          scene_upd_inst_trans(scene, offset, transform);
        }
      }
    }

#if false  
  if (active_scene_id == SCENE_DISCO)
  {
    uint8_t disco_offsets_cnt = sizeof(disco_offsets) / sizeof(disco_offsets[0]);

    // init
    if (active_scene_changed)
    {
      // TODO
    }

    // TODO
  }
#endif
#if false
    // DEBUG
    bool disabled = (((int)scene_time) % 2) > 0;
    for (uint8_t i = 0; i < yellow_sphere_offsets_cnt; i++)
    {
      uint8_t offset = yellow_sphere_offsets[i];

      if (disabled)
      {
        scene_set_inst_state(scene, offset, IS_DISABLED);
      }
      else
      {
        scene_clr_inst_state(scene, offset, IS_DISABLED);
      }
    }
#endif

  // reset active scene changed
  active_scene_changed = false;
}

__attribute__((visibility("default"))) bool update(float time, bool converge, bool run_track)
{
  bool finished = sync_is_finished(&intro, time);
  if (run_track && !finished)
    process_events(&intro, time);

  handle_animations(&intro, time);
  renderer_update(active_scene, active_post ? &post : NULL, converge);
  set_ltri_cnt(active_scene->ltri_cnt);

  return finished;
}

#ifndef TINY_BUILD
__attribute__((visibility("default"))) void release()
{
  sync_release_track(&intro);

  for (uint8_t i = 0; i < scene_cnt; i++)
    scene_release(&scenes[i]);
  free(scenes);
}
#endif
