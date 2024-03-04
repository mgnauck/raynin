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
#define NO_KEY_OR_MOUSE_HANDLING
#define WIDTH       1280
#define HEIGHT      800
#endif

#define RIOW_SIZE   22
#define TRI_CNT     800 + 2
#define MESH_CNT    2
#define INST_CNT    (RIOW_SIZE * RIOW_SIZE + 4)
#define MTL_CNT     INST_CNT

scene         scn;
scene         *cs = &scn;
render_data   *rd;

bool          orbit_cam = false;
bool          paused = false;

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

  s->cam = (cam){ .vert_fov = 20.0f, .foc_dist = 10.0f, .foc_angle = 0.6f };
  cam_set(&s->cam, (vec3){ 13.0f, 2.0f, 3.0f }, (vec3){ 0.0f, 0.0f, 0.0f });

  // Meshes
  scene_add_quad(s, (vec3){ 0.0f, 0.0f, 0.0f }, (vec3){ 0.0f, 1.0f, 0.0f }, 40.0f, 40.0f);
  scene_add_uvsphere(s, 1.0f, 20, 20, false);
  //scene_add_icosphere(s, 0, true);
  //scene_add_mesh(s, icosahedron);
  //scene_add_uvcylinder(s, 1.0f, 2.0f, 20, 20, false);
  scene_build_bvhs(s);

  mat4 translation;
  uint32_t mtl_id;

  // Floor instance
  mat4_trans(translation, (vec3){ 0.0f, 0.0f, 0.0f });
  mtl_id = scene_add_mtl(s, &(mtl){ .color = { 0.5f, 0.5f, 0.5f }, .value = 0.0f });
  scene_add_inst(s, 0, mtl_id, translation); 

  // Sphere instances
  mat4_trans(translation, (vec3){ 4.0f, 1.0f, 0.0f });
  mtl_id = scene_add_mtl(s, &(mtl){ .color = { 0.7f, 0.6f, 0.5f }, .value = 0.001f });
  scene_add_inst(s, 1, mtl_id, translation);

  mat4_trans(translation, (vec3){ 0.0f, 1.0f, 0.0f });
  mtl_id = scene_add_mtl(s, &(mtl){ .color = { 1.0f, 1.0f, 1.0f }, .value = 1.5f });
  scene_add_inst(s, 1, mtl_id, translation);

  mat4_trans(translation, (vec3){ -4.0f, 1.0f, 0.0f });
  mtl_id = scene_add_mtl(s, &(mtl){ .color = { 0.4f, 0.2f, 0.1f }, .value = 0.0f });
  scene_add_inst(s, 1, mtl_id, translation);

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
        scene_add_inst(s, 1, mtl_id, transform);
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
  renderer_set_bg_col(rd, (vec3){ 0.7f, 0.8f, 1.0f });
  renderer_update_static(rd);
}

void update_scene(scene *s, float time)
{
  // Update camera
  if(orbit_cam) {
    float v = 0.5f;
    float r = 16.0f;
    float h = 2.0f;
    vec3 pos = (vec3){ r * sinf(time * v * v), 0.2f + h + h * sinf(time * v * 0.7f), r * cosf(time * v) };
    cam_set(&s->cam, pos, vec3_neg(pos));
    scene_set_dirty(s, RT_CAM_VIEW);
  }

  // Update instances
  // ..
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

#ifndef NO_KEY_OR_MOUSE_HANDLING
  if(SDL_SetRelativeMouseMode(SDL_TRUE) < 0) {
    code = EXIT_FAILURE;
    goto clean_window;
  }
#endif

  init(WIDTH, HEIGHT);

  bool      quit = false;
  uint64_t  start = SDL_GetTicks64();
  uint64_t  last = start;

  while(!quit)
  {
    SDL_Event event;
    while(SDL_PollEvent(&event)) {
      if(event.type == SDL_QUIT)
        quit = true;
#ifndef NO_KEY_OR_MOUSE_HANDLING
      else if(event.type == SDL_KEYDOWN)
        handle_keypress(&event.key.keysym);
      else if(event.type == SDL_MOUSEMOTION)
        handle_mouse_motion(&event.motion);
#endif
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
