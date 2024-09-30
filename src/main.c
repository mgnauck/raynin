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
uint32_t  active_cam = 0;

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
      active_cam = active_cam > 0 ? active_cam - 1 : active_scene->cam_cnt - 1;
      scene_set_active_cam(active_scene, active_cam);
      logc("Setting cam %i of %i cams active", active_cam, active_scene->cam_cnt);
      break;
    case 'm':
      active_cam = (active_cam + 1) % active_scene->cam_cnt;
      scene_set_active_cam(active_scene, active_cam);
      logc("Setting cam %i of %i cams active", active_cam, active_scene->cam_cnt);
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
/*void init_scene_riow(scene *s)
{
#define RIOW_SIZE   22
#define LIGHT_CNT   3
#define MESH_CNT    2 + LIGHT_CNT
#define INST_CNT    (RIOW_SIZE * RIOW_SIZE + 4 + 3)
#define MTL_CNT     INST_CNT

  scene_init(s, MESH_CNT, MTL_CNT, 1, INST_CNT);

  cam *c = scene_get_active_cam(s);
  *c = (cam){ .vert_fov = 20.0f, .foc_dist = 11.0f, .foc_angle = 0.0f };
  cam_set(c, (vec3){ 13.0f, 2.0f, 3.0f }, (vec3){ 0.0f, 0.0f, 0.0f });

  uint16_t mtl_id = 0;
  mtl m;
  mesh *mesh;
  mat4 transform, translation, rotation, scale;

  // Light meshes (need to be unique)
  uint16_t lid = 0;
  m = mtl_init((vec3){ 1.16f, 1.16f, 1.16f });
  m.emissive = 1.0f;
  mtl_id = scene_add_mtl(s, &m);
  mesh = scene_acquire_mesh(s);
  mesh_create_quad(mesh, 0, 1, mtl_id, false);
  lid = scene_attach_mesh(s, mesh, true);
  for(uint8_t i=1; i<LIGHT_CNT; i++) {
    mesh = scene_acquire_mesh(s);
    mesh_create_quad(mesh, 1, 1, mtl_id, false);
    scene_attach_mesh(s, mesh, true);
  }

  // Sphere mesh
  uint16_t sid = 0;
  mesh = scene_acquire_mesh(s);
  mesh_create_sphere(mesh, 1.0f, 20, 20, mtl_id, false, false);
  sid = scene_attach_mesh(s, mesh, false);

  // Floor mesh
  uint16_t fid = 0;
  m = mtl_init((vec3){ 0.25f, 0.25f, 0.25f });
  m.metallic = 0.0f;
  m.roughness = 1.0f;
  mtl_id = scene_add_mtl(s, &m);
  mesh = scene_acquire_mesh(s);
  mesh_create_quad(mesh, 10, 10, mtl_id, false);
  fid = scene_attach_mesh(s, mesh, false);

  // Light instances
  for(uint8_t i=0; i<LIGHT_CNT; i++) {
    mat4_scale_u(scale, 2.5f);
    mat4_rot_x(rotation, PI);
    mat4_mul(transform, scale, rotation);
    mat4_trans(translation, (vec3){ 0.0f, 5.0f, -10.0f + (i * 10.0f) });
    //mat4_trans(translation, (vec3){ 0.0f, 5.0f, (i * 10.0f) });
    mat4_mul(transform, translation, transform);
    scene_add_inst(s, lid + i, NO_MTL_OVERRIDE, 0, transform);
  }

  // Floor instance
  mat4_scale_u(scale, 12.0f);
  scene_add_inst(s, fid, NO_MTL_OVERRIDE, 0, scale);

  // Reflecting sphere (black)
  mat4_scale(scale, (vec3){ 1.0, 1.0, 1.0 });
  mat4_trans(translation, (vec3){ 4.0f, 1.0f, 0.0f });
  mat4_mul(translation, translation, scale);
  m = mtl_init((vec3){ 1.0f, 1.0f, 1.0f });
  m.ior = 2.3f;
  m.roughness = 0.0f;
  mtl_id = scene_add_mtl(s, &m);
  scene_add_inst(s, sid, mtl_id, 0, translation);

  // Reflecting sphere (white)
  mat4_scale(scale, (vec3){ 1.0, 1.0, 1.0 });
  mat4_trans(translation, (vec3){ 0.0f, 1.0f, 0.0f });
  mat4_mul(translation, translation, scale);
  m = mtl_init((vec3){ 1.0f, 1.0f, 1.0f });
  m.ior = 1.5f;
  m.roughness = 0.075f;
  mtl_id = scene_add_mtl(s, &m);
  scene_add_inst(s, sid, mtl_id, 0, translation);

  // Metallic sphere
  mat4_scale(scale, (vec3){ 1.0, 1.0, 1.0 });
  mat4_trans(translation, (vec3){ -4.0f, 1.0f, 0.0f });
  mat4_mul(translation, translation, scale);
  m = mtl_init((vec3){ 0.98f, 0.85f, 0.72f });
  m.metallic = 1.0f;
  m.roughness = 0.5f;
  mtl_id = scene_add_mtl(s, &m);
  scene_add_inst(s, sid, mtl_id, 0, translation);

  mat4_scale_u(scale, 0.2f);

  for(int a=-RIOW_SIZE/2; a<RIOW_SIZE/2; a++) {
    for(int b=-RIOW_SIZE/2; b<RIOW_SIZE/2; b++) {
      vec3 center = { (float)a + 0.9f * pcg_randf(), 0.2f, (float)b + 0.9f * pcg_randf() };
      if(vec3_len(vec3_add(center, (vec3){ -4.0f, -0.2f, 0.0f })) > 0.9f) {
        m = mtl_init(vec3_rand());
        m.roughness = 0.075f;
        if(pcg_randf() < 0.4f) {
          m.refractive = 1.0f;
          m.roughness = pcg_randf();
          m.ior = 1.01 + pcg_randf();
        }
        mtl_id = scene_add_mtl(s, &m);
        mat4_trans(translation, center);
        mat4_mul(transform, translation, scale);
        scene_add_inst(s, sid, mtl_id, 0, transform);
      }
    }
  }

  scene_finalize(s);
}*/

__attribute__((visibility("default")))
uint8_t load_scene_gltf(const char *gltf, size_t gltf_sz, const unsigned char *bin, size_t bin_sz, bool prepare_for_export)
{
  uint8_t ret = 0;

  if(scenes == NULL)
    scenes = malloc(MAX_SCENES * sizeof(*scenes));

  // Scene to load
  scene *s = scenes + scene_cnt;

  //init_scene_riow(s);

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
uint8_t load_scenes_bin(uint8_t *bin)
{
#ifndef LOAD_EMBEDDED_DATA
  return import_bin(&scenes, &scene_cnt, bin);
#else
  return 0;
#endif
}

__attribute__((visibility("default")))
void init()
{
#ifdef LOAD_EMBEDDED_DATA
  import_bin(&scenes, &scene_cnt, scene_data);
#endif

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


__attribute__((visibility("default")))
void update(float time, bool converge)
{
  /*
  static float last_change = 0.0;
  if(time - last_change > 2.0) {
    active_scene_id = (active_scene_id + 1) % scene_cnt;
    active_scene = &scenes[active_scene_id];
    scene_set_dirty(active_scene, RT_CFG | RT_CAM | RT_MTL | RT_TRI | RT_LTRI | RT_INST | RT_BLAS);
    last_change = time;
  }
  */

  renderer_update(active_scene, converge);
  set_ltri_cnt(active_scene->ltri_cnt);
}

#ifndef TINY_BUILD
__attribute__((visibility("default")))
void release()
{
  for(uint8_t i=0; i<scene_cnt; i++)
    scene_release(&scenes[i]);
  free(scenes);
  scenes = NULL;
  scene_cnt = 0;
}
#endif
