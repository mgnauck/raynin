#include <stdbool.h>
#include <stdint.h>
#include "mutil.h"
#include "vec3.h"
#include "mtl.h"
#include "mesh.h"
#include "inst.h"
#include "tlas.h"
#include "scene.h"
#include "renderer.h"
#include "import.h"
#include "settings.h"
#include "log.h"

#include "data/teapot.h"
#include "data/dragon.h"
#include "data/icosahedron.h"

#ifdef NATIVE_BUILD
#include <SDL.h>
#define WIDTH       800
#define HEIGHT      600
#endif

#define RIOW_SIZE   22
#define TRI_CNT     1280 + 600
#define LIGHT_CNT   3
#define LTRI_CNT    LIGHT_CNT * 2
#define MESH_CNT    2 + LIGHT_CNT
#define INST_CNT    (RIOW_SIZE * RIOW_SIZE + 4 + 3)
#define MTL_CNT     INST_CNT

scene         scn;
scene         *cs = &scn;
render_data   *rd;
bool          orbit_cam = false;

  __attribute__((visibility("default")))
void key_down(unsigned char key, float move_vel)
{
  float speed = vec3_max_comp(vec3_scale(vec3_sub(cs->tlas_nodes[0].max, cs->tlas_nodes[0].min), 0.2f));

  switch(key) {
    case 'a':
      cs->cam.eye = vec3_add(cs->cam.eye, vec3_scale(cs->cam.right, -speed * move_vel));
      break;
    case 'd':
      cs->cam.eye = vec3_add(cs->cam.eye, vec3_scale(cs->cam.right, speed * move_vel));
      break;
    case 'w':
      cs->cam.eye = vec3_add(cs->cam.eye, vec3_scale(cs->cam.fwd, -speed * move_vel));
     break;
    case 's':
      cs->cam.eye = vec3_add(cs->cam.eye, vec3_scale(cs->cam.fwd, speed * move_vel));
      break;
    case 'i':
      cs->cam.foc_dist += 0.1f;
      break;
    case 'k':
      cs->cam.foc_dist = max(cs->cam.foc_dist - 0.1f, 0.1f);
      break;
    case 'j':
      cs->cam.foc_angle = max(cs->cam.foc_angle - 0.1f, 0.1f);
      break;
    case 'l':
      cs->cam.foc_angle += 0.1f;
      break;
    case 'o':
      orbit_cam = !orbit_cam;
      break;
    case 'u':
      cs->bg_col = vec3_sub((vec3){1.0f, 1.0f, 1.0f }, cs->bg_col);
      break;
    case 'r':
      // TODO Call JS to reload shader
      break;
  }

  scene_set_dirty(cs, RT_CAM_VIEW);
}

__attribute__((visibility("default")))
void mouse_move(int32_t dx, int32_t dy, float look_vel)
{
  float theta = min(max(acosf(-cs->cam.fwd.y) + (float)dy * look_vel, 0.01f), 0.99f * PI);
  float phi = fmodf(atan2f(-cs->cam.fwd.z, cs->cam.fwd.x) + PI - (float)dx * look_vel, 2.0f * PI);
  
  cam_set_dir(&cs->cam, vec3_spherical(theta, phi));
  
  scene_set_dirty(cs, RT_CAM_VIEW);
}

void init_scene_riow(scene *s)
{
  scene_init(s, MESH_CNT, MTL_CNT, INST_CNT);

  s->cam = (cam){ .vert_fov = 20.0f, .foc_dist = 11.0f, .foc_angle = 0.5f };
  cam_set(&s->cam, (vec3){ 13.0f, 2.0f, 3.0f }, (vec3){ 0.0f, 0.0f, 0.0f });

  uint16_t mtl_id = 0;
  uint32_t lid = 0;
  uint32_t sid = 0;
  uint32_t fid = 0;
  mesh *mesh = 0;
  mtl m;
  mat4 transform, translation, rotation, scale;

  // Light meshes (need to be unique)
  m = mtl_init((vec3){ 1.16f, 1.16, 1.16f });
  mtl_id = scene_add_mtl(s, &m);
  mesh = scene_acquire_mesh(s);
  mesh_create_quad(mesh, 1, 1, mtl_id);
  lid = scene_attach_mesh(s, mesh, true);
  for(uint8_t i=1; i<LIGHT_CNT; i++) {
    mesh = scene_acquire_mesh(s);
    mesh_create_quad(mesh, 1, 1, mtl_id);
    scene_attach_mesh(s, mesh, true);
  }

  // Sphere mesh
  mesh = scene_acquire_mesh(s);
  mesh_create_uvsphere(mesh, 1.0f, 20, 20, mtl_id, false);
  //mesh_create_uvcylinder(mesh, 1.0f, 2.0f, 20, 20, 1, false);
  //mesh_create_icosphere(mesh, 3, mtl_id, false);
  //import_mesh(mesh, teapot, mtl_id, false);
  sid = scene_attach_mesh(s, mesh, false);
  
  // Floor mesh
  m = mtl_init((vec3){ 0.25f, 0.25f, 0.25f });
  m.metallic = 0.0f;
  m.roughness = 1.0f;
  mtl_id = scene_add_mtl(s, &m);
  mesh = scene_acquire_mesh(s);
  mesh_create_quad(mesh, 10, 10, mtl_id);
  fid = scene_attach_mesh(s, mesh, false);
 
  // Light instances
  for(uint8_t i=0; i<LIGHT_CNT; i++) { 
    mat4_scale_u(scale, 2.5f);
    mat4_rot_x(rotation, PI);
    mat4_mul(transform, scale, rotation);
    mat4_trans(translation, (vec3){ 0.0f, 5.0f, -10.0f + (i * 10.0f) });
    //mat4_trans(translation, (vec3){ 0.0f, 5.0f, (i * 10.0f) });
    mat4_mul(transform, translation, transform);
    scene_add_mesh_inst(s, lid + i, -1, transform);
  }

  // Floor instance
  mat4_scale_u(scale, 12.0f);
  //scene_add_shape_inst(s, ST_PLANE, mtl_id, scale);
  scene_add_mesh_inst(s, fid, -1, scale);
 
  // Reflecting sphere (black)
  mat4_scale(scale, (vec3){ 1.0, 1.0, 1.0 });
  mat4_trans(translation, (vec3){ 4.0f, 1.0f, 0.0f });
  mat4_mul(translation, translation, scale);
  m = mtl_init((vec3){ 0.0f, 0.0f, 0.0f });
  m.ior = 2.3f;
  mtl_id = scene_add_mtl(s, &m);
  //scene_add_shape_inst(s, ST_SPHERE, mtl_id, translation);
  scene_add_mesh_inst(s, sid, mtl_id, translation);

  // Reflecting sphere (white)
  mat4_scale(scale, (vec3){ 1.0, 1.0, 1.0 });
  mat4_trans(translation, (vec3){ 0.0f, 1.0f, 0.0f });
  mat4_mul(translation, translation, scale);
  m = mtl_init((vec3){ 1.0f, 1.0f, 1.0f });
  m.ior = 1.5f;
  mtl_id = scene_add_mtl(s, &m);
  //scene_add_shape_inst(s, ST_SPHERE, mtl_id, translation);
  scene_add_mesh_inst(s, sid, mtl_id, translation);

  // Metallic sphere
  mat4_scale(scale, (vec3){ 1.0, 1.0, 1.0 });
  mat4_trans(translation, (vec3){ -4.0f, 1.0f, 0.0f });
  mat4_mul(translation, translation, scale);
  m = mtl_init((vec3){ 0.98f, 0.85f, 0.72f });
  m.metallic = 1.0f;
  m.roughness = 0.5f;
  mtl_id = scene_add_mtl(s, &m);
  //scene_add_shape_inst(s, ST_SPHERE, mtl_id, translation);
  scene_add_mesh_inst(s, sid, mtl_id, translation);

  mat4_scale_u(scale, 0.2f);
  
  for(int a=-RIOW_SIZE/2; a<RIOW_SIZE/2; a++) {
    for(int b=-RIOW_SIZE/2; b<RIOW_SIZE/2; b++) {
      vec3 center = { (float)a + 0.9f * pcg_randf(), 0.2f, (float)b + 0.9f * pcg_randf() };
      if(vec3_len(vec3_add(center, (vec3){ -4.0f, -0.2f, 0.0f })) > 0.9f) {
        m = mtl_init(vec3_rand());
        if(pcg_randf() < 0.4f) {
          m.refractive = 1.0f;
          m.roughness = pcg_randf();
          m.ior = 1.01 + pcg_randf();
        }
        mtl_id = scene_add_mtl(s, &m);
        mat4_trans(translation, center);
        mat4_mul(transform, translation, scale);
        //scene_add_shape_inst(s, ST_SPHERE, mtl_id, transform);
        scene_add_mesh_inst(s, sid, mtl_id, transform);
      }
    }
  }

  scene_finalize(s);
}

__attribute__((visibility("default")))
uint8_t init_scene(const char *gltf, size_t gltf_sz, const unsigned char *bin, size_t bin_sz)
{
  return import_gltf(cs, gltf, gltf_sz, bin, bin_sz);
}

__attribute__((visibility("default")))
void init(uint32_t width, uint32_t height, uint8_t spp)
{
  pcg_srand(42u, 303u);

  //init_scene_riow(cs);

  logc("max tris: %i, max ltris: %i, max mtls: %i, max insts: %i",
      cs->max_tri_cnt, cs->max_ltri_cnt, cs->max_mtl_cnt, cs->max_inst_cnt);
  
  renderer_gpu_alloc(cs->max_tri_cnt, cs->max_ltri_cnt, cs->max_mtl_cnt, cs->max_inst_cnt);
  rd = renderer_init(cs, width, height, spp);

  renderer_update_static(rd);
}

void update_scene(scene *s, float time)
{
  // Set scene always dirty, i.e. do not converge
  //scene_set_dirty(s, RT_CAM_VIEW);

  // Update camera
  if(orbit_cam) {
    vec3  e = vec3_scale(vec3_sub(s->tlas_nodes[0].max, s->tlas_nodes[0].min), 0.4f);
    vec3 pos = (vec3){ e.x * sinf(time * 0.25f), 0.25f + e.y + e.y * sinf(time * 0.35f), e.z * cosf(time * 0.5f) };
    cam_set(&s->cam, pos, vec3_neg(pos));
    scene_set_dirty(s, RT_CAM_VIEW);
  }
}

__attribute__((visibility("default")))
void update(float time)
{
  update_scene(cs, time);
  renderer_update(rd, time);
}

__attribute__((visibility("default")))
void release()
{
  renderer_release(rd);
  scene_release(&scn);
}

#ifdef NATIVE_BUILD

const char *gltf = "{  \"scenes\": [    {      \"extras\": {        \"bgcol\": [          0.7,          0.7,          0.7        ]      },          }  ],  \"nodes\": [    {      \"mesh\": 0,      \"name\": \"Sphere\",      \"scale\": [        10,        10,        10      ],      \"translation\": [        0,        0,        0      ]    },    {      \"camera\": 0,      \"name\": \"Camera\",      \"rotation\": [        -0.09568335115909576,        0.4728190302848816,        0.04415176808834076,        0.874835729598999      ],      \"translation\": [        50,        5,        50      ]    }  ],  \"cameras\": [    {      \"name\": \"Camera.003\",      \"perspective\": {        \"aspectRatio\": 1.7777777777777777,        \"yfov\": 0.39959651231765747,        \"zfar\": 1000,        \"znear\": 0.10000000149011612      },      \"type\": \"perspective\"    }  ],  \"materials\": [    {      \"doubleSided\": true,      \"name\": \"Material.001\",      \"pbrMetallicRoughness\": {        \"baseColorFactor\": [          1,          0,          0,          1        ],              }    },      ],  \"meshes\": [    {      \"name\": \"Sphere\",      \"extras\": {        \"subx\": 36,        \"suby\": 36      },      \"primitives\": [        {          \"attributes\": {            \"POSITION\": 6,            \"NORMAL\": 7          },          \"indices\": 8,          \"material\": 0        }      ]    }  ]}";

int main(int argc, char *argv[])
{
  int32_t code = EXIT_SUCCESS;
 
  if(SDL_Init(SDL_INIT_VIDEO) < 0)
    return EXIT_FAILURE;
  
  SDL_Window *window = SDL_CreateWindow("unik",
      SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WIDTH, HEIGHT, 0);
  if(!window) {
    code = EXIT_FAILURE;
    goto clean_sdl;
  }

  SDL_Surface *screen = SDL_GetWindowSurface(window);
  if(!screen) {
    code = EXIT_FAILURE;
    goto clean_window;
  }

  init_scene(gltf, strlen(gltf), 0, 0);
  init(WIDTH, HEIGHT, 1);

  bool      quit = false;
  bool      lock_mouse = false;
  uint64_t  start = SDL_GetTicks64();
  uint64_t  last = start;

  while(!quit)
  {
    SDL_Event event;
    while(SDL_PollEvent(&event)) {
      if(event.type == SDL_QUIT)
        quit = true;
      else if((event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) ||
          (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE)) {
        if(!lock_mouse && event.type == SDL_KEYDOWN)
          quit = true;
        lock_mouse = !lock_mouse;
        if(SDL_SetRelativeMouseMode(lock_mouse ? SDL_TRUE : SDL_FALSE) < 0) {
          code = EXIT_FAILURE;
          goto clean_window;
        }
      }
      else if(event.type == SDL_KEYDOWN)
        key_down(event.key.keysym.sym, 0.1f);
      else if(lock_mouse && event.type == SDL_MOUSEMOTION)
        mouse_move(event.motion.xrel, event.motion.yrel, 0.005f);
    }

    uint64_t frame = SDL_GetTicks64() - last;
    char title[256];
    snprintf(title, 256, "%ld ms / %6.3f / %4.2fM rays/s", frame, 1000.0f / frame, WIDTH * HEIGHT / (frame * 1000.0f));
    SDL_SetWindowTitle(window, title);
    last = SDL_GetTicks64();

    update((last - start) / 1000.0f);
    renderer_render(rd, screen);

    SDL_UpdateWindowSurface(window);
  }

  release();

clean_window:
  SDL_DestroyWindow(window);
clean_sdl:
  SDL_Quit();
 
  return code;
}

#endif
