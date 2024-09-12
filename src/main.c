#include <stdbool.h>
#include <stdint.h>
#include "imex/import.h"
#include "rend/bvh.h"
#include "rend/renderer.h"
#include "scene/cam.h"
#include "scene/mesh.h"
#include "scene/mtl.h"
#include "scene/scene.h"
#include "sys/log.h"
#include "sys/mutil.h"
#include "sys/sutil.h"
#include "util/vec3.h"

scene     *s = 0;
uint32_t  active_cam = 0;
bool      orbit_cam = false;

extern void toggle_converge();
extern void toggle_filter();
extern void toggle_reprojection();

  __attribute__((visibility("default")))
void key_down(unsigned char key, float move_vel)
{
  vec3 gmin = vec3_min(s->tlas_nodes[0].lmin, s->tlas_nodes[0].rmin);
  vec3 gmax = vec3_max(s->tlas_nodes[0].lmax, s->tlas_nodes[0].rmax);

  float speed = vec3_max_comp(vec3_scale(vec3_sub(gmax, gmin), 0.2f));

  cam *cam = scene_get_active_cam(s);

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
    case 'o':
      orbit_cam = !orbit_cam;
      break;
    case 'u':
      s->bg_col = vec3_sub((vec3){1.0f, 1.0f, 1.0f }, s->bg_col);
      scene_set_dirty(s, RT_CFG);
      break;
    case 'm':
      active_cam = (active_cam + 1) % s->cam_cnt;
      scene_set_active_cam(s, active_cam);
      logc("Setting cam %i of %i cams active", active_cam, s->cam_cnt);
      break;
    case 'n':
      active_cam = active_cam > 0 ? active_cam - 1 : s->cam_cnt - 1;
      scene_set_active_cam(s, active_cam);
      logc("Setting cam %i of %i cams active", active_cam, s->cam_cnt);
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

  scene_set_dirty(s, RT_CAM);
}

__attribute__((visibility("default")))
void mouse_move(int32_t dx, int32_t dy, float look_vel)
{
  cam *cam = scene_get_active_cam(s);

  float theta = min(max(acosf(-cam->fwd.y) + (float)dy * look_vel, 0.05f), 0.95f * PI);
  float phi = fmodf(atan2f(-cam->fwd.z, cam->fwd.x) + PI - (float)dx * look_vel, 2.0f * PI);

  cam_set_dir(cam, vec3_spherical(theta, phi));

  scene_set_dirty(s, RT_CAM);
}

void init_scene_riow(scene *s)
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
  uint32_t lid = 0;
  m = mtl_init((vec3){ 1.16f, 1.16f, 1.16f });
  m.emissive = 1.0f;
  mtl_id = scene_add_mtl(s, &m);
  mesh = scene_acquire_mesh(s);
  mesh_create_quad(mesh, 1, 1, mtl_id, false);
  lid = scene_attach_mesh(s, mesh, true);
  for(uint8_t i=1; i<LIGHT_CNT; i++) {
    mesh = scene_acquire_mesh(s);
    mesh_create_quad(mesh, 1, 1, mtl_id, false);
    scene_attach_mesh(s, mesh, true);
  }

  // Sphere mesh
  uint32_t sid = 0;
  mesh = scene_acquire_mesh(s);
  mesh_create_sphere(mesh, 1.0f, 20, 20, mtl_id, false, false);
  sid = scene_attach_mesh(s, mesh, false);

  // Floor mesh
  uint32_t fid = 0;
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
    scene_add_inst(s, lid + i, -1, transform);
  }

  // Floor instance
  mat4_scale_u(scale, 12.0f);
  scene_add_inst(s, fid, -1, scale);

  // Reflecting sphere (black)
  mat4_scale(scale, (vec3){ 1.0, 1.0, 1.0 });
  mat4_trans(translation, (vec3){ 4.0f, 1.0f, 0.0f });
  mat4_mul(translation, translation, scale);
  m = mtl_init((vec3){ 0.0f, 0.0f, 0.0f });
  m.ior = 2.3f;
  m.roughness = 0.0f;
  mtl_id = scene_add_mtl(s, &m);
  scene_add_inst(s, sid, mtl_id, translation);

  // Reflecting sphere (white)
  mat4_scale(scale, (vec3){ 1.0, 1.0, 1.0 });
  mat4_trans(translation, (vec3){ 0.0f, 1.0f, 0.0f });
  mat4_mul(translation, translation, scale);
  m = mtl_init((vec3){ 1.0f, 1.0f, 1.0f });
  m.ior = 1.5f;
  m.roughness = 0.0f;
  mtl_id = scene_add_mtl(s, &m);
  scene_add_inst(s, sid, mtl_id, translation);

  // Metallic sphere
  mat4_scale(scale, (vec3){ 1.0, 1.0, 1.0 });
  mat4_trans(translation, (vec3){ -4.0f, 1.0f, 0.0f });
  mat4_mul(translation, translation, scale);
  m = mtl_init((vec3){ 0.98f, 0.85f, 0.72f });
  m.metallic = 1.0f;
  m.roughness = 0.5f;
  mtl_id = scene_add_mtl(s, &m);
  scene_add_inst(s, sid, mtl_id, translation);

  mat4_scale_u(scale, 0.2f);

  for(int a=-RIOW_SIZE/2; a<RIOW_SIZE/2; a++) {
    for(int b=-RIOW_SIZE/2; b<RIOW_SIZE/2; b++) {
      vec3 center = { (float)a + 0.9f * pcg_randf(), 0.2f, (float)b + 0.9f * pcg_randf() };
      if(vec3_len(vec3_add(center, (vec3){ -4.0f, -0.2f, 0.0f })) > 0.9f) {
        m = mtl_init(vec3_rand());
        m.roughness = 0.0f;
        if(pcg_randf() < 0.4f) {
          m.refractive = 1.0f;
          m.roughness = pcg_randf();
          m.ior = 1.01 + pcg_randf();
        }
        mtl_id = scene_add_mtl(s, &m);
        mat4_trans(translation, center);
        mat4_mul(transform, translation, scale);
        scene_add_inst(s, sid, mtl_id, transform);
      }
    }
  }

  scene_finalize(s);
}

__attribute__((visibility("default")))
uint8_t init(const char *gltf, size_t gltf_sz, const unsigned char *bin, size_t bin_sz)
{
  uint8_t ret = 0;

  s = malloc(sizeof(*s));

  //init_scene_riow(s);
  ret = import_gltf(s, gltf, gltf_sz, bin, bin_sz);

  logc("max tris: %i, max ltris: %i, max mtls: %i, max insts: %i",
      s->max_tri_cnt, s->max_ltri_cnt, s->max_mtl_cnt, s->max_inst_cnt);

  // Allocate resource on the GPU
  if(ret == 0)
    renderer_gpu_alloc(s->max_tri_cnt, s->max_ltri_cnt, s->max_mtl_cnt, s->max_inst_cnt);

  return ret;
}

__attribute__((visibility("default")))
void update(float time, bool converge)
{
  // Update camera
  if(s->tlas_nodes && orbit_cam) {
    cam *cam = scene_get_active_cam(s);
    vec3 gmin = vec3_min(s->tlas_nodes[0].lmin, s->tlas_nodes[0].rmin);
    vec3 gmax = vec3_max(s->tlas_nodes[0].lmax, s->tlas_nodes[0].rmax);
    //vec3  e = vec3_scale(vec3_sub(gmax, gmin), 0.6f);
    vec3  e = vec3_scale(vec3_sub(gmax, gmin), 0.27f);
    //vec3 pos = (vec3){ e.x * sinf(time * 0.25f), 0.25f + e.y + e.y * sinf(time * 0.35f), e.z * cosf(time * 0.5f) };
    vec3 pos = (vec3){ e.x * sinf(time * 0.25f), 20.0f, e.z * cosf(time * 0.25f) };
    //cam_set(cam, pos, vec3_neg(pos));
    cam_set(cam, pos, vec3_add((vec3){0.0f, 24.0, 0.0f}, vec3_neg(pos)));
    scene_set_dirty(s, RT_CAM);
  }

  /*mat4 translation;
  mat4_trans(translation, (vec3){ cosf(time * 0.4f) * 3.0f, 0.5f + sinf(time * 0.2f) * 1.0, sinf(time * 0.3f) * 3.0f });
  scene_upd_inst_trans(s, 6, translation);*/

  renderer_update(s, converge);
}

__attribute__((visibility("default")))
void release()
{
  scene_release(s);
  free(s);
}
