#include <stdbool.h>
#include <stdint.h>
#include "mutil.h"
#include "vec3.h"
#include "mtl.h"
#include "scene.h"
#include "renderer.h"
#include "settings.h"

#include "data/teapot.h"
#include "data/dragon.h"
#include "data/icosahedron.h"

#ifdef NATIVE_BUILD
#include <SDL.h>
#define WIDTH       800
#define HEIGHT      600
#endif

#define RIOW_SIZE   22
#define TRI_CNT     1280 + 4
#define MESH_CNT    3
#define INST_CNT    (RIOW_SIZE * RIOW_SIZE + 4 + 1)
#define MTL_CNT     INST_CNT

scene         scn;
scene         *cs = &scn;
render_data   *rd;

bool          orbit_cam = false;
bool          paused = true;

  __attribute__((visibility("default")))
void key_down(unsigned char key)
{
#define MOVE_VEL 0.1f

  switch(key) {
    case 'a':
      cs->cam.eye = vec3_add(cs->cam.eye, vec3_scale(cs->cam.right, -MOVE_VEL));
      break;
    case 'd':
      cs->cam.eye = vec3_add(cs->cam.eye, vec3_scale(cs->cam.right, MOVE_VEL));
      break;
    case 'w':
      cs->cam.eye = vec3_add(cs->cam.eye, vec3_scale(cs->cam.fwd, -MOVE_VEL));
     break;
    case 's':
      cs->cam.eye = vec3_add(cs->cam.eye, vec3_scale(cs->cam.fwd, MOVE_VEL));
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
    case 'p':
      paused = !paused;
      break;
    case 'r':
      // TODO Call JS to reload shader
      break;
  }

  scene_set_dirty(cs, RT_CAM_VIEW);
}

__attribute__((visibility("default")))
void mouse_move(int32_t dx, int32_t dy)
{
#define LOOK_VEL 0.015f
  
  float theta = min(max(acosf(-cs->cam.fwd.y) + (float)dy * LOOK_VEL, 0.01f), 0.99f * PI);
  float phi = fmodf(atan2f(-cs->cam.fwd.z, cs->cam.fwd.x) + PI - (float)dx * LOOK_VEL, 2.0f * PI);
  
  cam_set_dir(&cs->cam, vec3_spherical(theta, phi));
  
  scene_set_dirty(cs, RT_CAM_VIEW);
}

void init_scene_riow(scene *s)
{
  scene_init(s, MESH_CNT, MTL_CNT, INST_CNT);

  s->cam = (cam){ .vert_fov = 20.0f, .foc_dist = 11.0f, .foc_angle = 0.5f };
  cam_set(&s->cam, (vec3){ 13.0f, 2.0f, 3.0f }, (vec3){ 0.0f, 0.0f, 0.0f });

  uint16_t mtl_id;
  uint16_t mesh_id;
  mat4 transform, translation, scale;

  // Light mesh
  mtl m = mtl_create_emissive();
  mtl_id = scene_add_mtl(s, &m);
  uint16_t lid = scene_add_quad(s, (vec3){ 0.0f, 5.0f, 0.0f }, (vec3){ 0.0f, -1.0f, 0.0f }, 5.0f, 5.0f, mtl_id);

  // Sphere mesh
  uint16_t sid = scene_add_uvsphere(s, 1.0f, 20, 20, mtl_id, false);
  //uint16_t sid = scene_add_icosphere(s, 2, mtl_id, false);
  //uint16_t sid = scene_add_mesh(s, teapot, mtl_id, false);
  //uint16_t sid = scene_add_uvcylinder(s, 1.0f, 2.0f, 20, 20, 1, false);
  
  // Floor mesh
  m = mtl_create_lambert();
  m.color = (vec3){ 0.5f, 0.5f, 0.5f };
  mtl_id = scene_add_mtl(s, &m);
  uint16_t fid = scene_add_quad(s, (vec3){ 0.0f, 0.0f, 0.0f }, (vec3){ 0.0f, 1.0f, 0.0f }, 20.0f, 20.0f, mtl_id);
 
  // Light instance
  mat4_scale(scale, 1.0f);
  scene_add_inst_mesh(s, lid, -1, scale);
  
  // Floor instance
  mat4_scale(scale, 1.2f); // 1.2
  //scene_add_inst_shape(s, ST_PLANE, mtl_id, scale);
  scene_add_inst_mesh(s, fid, -1, scale);
 
  // Sphere instances
  mat4_trans(translation, (vec3){ 4.0f, 1.0f, 0.0f });
  m = mtl_create_metal();
  m.color = (vec3){ 0.7f, 0.6f, 0.5f };
  m.value = 0.001f;
  mtl_id = scene_add_mtl(s, &m);
  //scene_add_inst_shape(s, ST_SPHERE, mtl_id, translation);
  scene_add_inst_mesh(s, sid, mtl_id, translation);

  mat4_trans(translation, (vec3){ 0.0f, 1.0f, 0.0f });
  m = mtl_create_dielectric();
  mtl_id = scene_add_mtl(s, &m);
  //scene_add_inst_shape(s, ST_SPHERE, 1, translation);
  scene_add_inst_mesh(s, sid, mtl_id, translation);

  mat4_trans(translation, (vec3){ -4.0f, 1.0f, 0.0f });
  m = mtl_create_lambert();
  m.color = (vec3){ 0.1f, 0.2f, 0.4f };
  mtl_id = scene_add_mtl(s, &m);
  //scene_add_inst_shape(s, ST_SPHERE, mtl_id, translation);
  scene_add_inst_mesh(s, sid, mtl_id, translation);

  mat4_scale(scale, 0.2f);
  
  for(int a=-RIOW_SIZE/2; a<RIOW_SIZE/2; a++) {
    for(int b=-RIOW_SIZE/2; b<RIOW_SIZE/2; b++) {
      float mtl_p = pcg_randf();
      vec3 center = { (float)a + 0.9f * pcg_randf(), 0.2f, (float)b + 0.9f * pcg_randf() };
      if(vec3_len(vec3_add(center, (vec3){ -4.0f, -0.2f, 0.0f })) > 0.9f) {
        if(mtl_p < 0.8f) {
          m = mtl_create_lambert();
          mtl_id = scene_add_mtl(s, &m);
        } else if(mtl_p < 0.95f) {
          m = mtl_create_metal();
          mtl_id = scene_add_mtl(s, &m);
        } else {
          m = mtl_create_dielectric();
          mtl_id = scene_add_mtl(s, &m);
        }

        mat4_trans(translation, center);
        mat4_mul(transform, translation, scale);
        //scene_add_inst_shape(s, ST_SPHERE, mtl_id, transform);
        scene_add_inst_mesh(s, sid, mtl_id, transform);
      }
    }
  }
}

__attribute__((visibility("default")))
void init(uint32_t width, uint32_t height)
{
  pcg_srand(42u, 303u);

  init_scene_riow(cs);

  renderer_gpu_alloc(TRI_CNT, MTL_CNT, INST_CNT);

  rd = renderer_init(cs, width, height, 2);
  //renderer_set_bg_col(rd, (vec3){ 0.0f, 0.0f, 0.0f });
  renderer_set_bg_col(rd, (vec3){ 0.07f, 0.08f, 0.1f });
  //renderer_set_bg_col(rd, (vec3){ 0.7f, 0.8f, 1.0f });
  renderer_update_static(rd);
}

void update_scene(scene *s, float time)
{
  // Update camera
  if(orbit_cam) {
    float v = 0.5f;
    float r = 16.0f;
    float h = 2.0f;
    vec3 pos = (vec3){ r * sinf(time * v * v), 1.2f + h + h * sinf(time * v * 0.7f), r * cosf(time * v) };
    cam_set(&s->cam, pos, vec3_neg(pos));
    scene_set_dirty(s, RT_CAM_VIEW);
  }

  if(!paused) {
    // Update instances
    mat4 translation;
    mat4_trans(translation, (vec3){ 4.0f, 2.0f + sinf(time * 0.6f), 0.0f });
    scene_upd_inst(s, 1, -1, translation);
    mat4_trans(translation, (vec3){ 0.0f, 2.0f + sinf(time * 0.6f + 0.6f), 0.0f });
    scene_upd_inst(s, 2, -1, translation);
    mat4_trans(translation, (vec3){ -4.0f, 2.0f + sinf(time * 0.6f + 1.2f), 0.0f });
    scene_upd_inst(s, 3, -1, translation);
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

  init(WIDTH, HEIGHT);

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
        key_down(event.key.keysym.sym);
      else if(lock_mouse && event.type == SDL_MOUSEMOTION)
        mouse_move(event.motion.xrel, event.motion.yrel);
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
