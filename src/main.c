#include <stdbool.h>
#include <stdint.h>
#include "sutil.h"
#include "mutil.h"
#include "buf.h"
#include "vec3.h"
#include "ray.h"
#include "tri.h"
#include "cfg.h"
#include "mesh.h"
#include "bvh.h"
#include "inst.h"
#include "mat.h"
#include "tlas.h"
#include "scene.h"
#include "intersect.h"
#include "log.h"
#include "teapot.h"
#include "dragon.h"

#define TRI_CNT         (1024 + 19332)
#define MESH_CNT        2
#define INST_CNT        32
#define MAT_CNT         32

uint32_t  gathered_smpls = 0;
vec3      bg_col = { 0.6f, 0.7f, 0.9f };

bool      orbit_cam = false;
bool      paused = false;

cfg       config;
scene     scn;

vec3      positions[INST_CNT];
vec3      directions[INST_CNT];
vec3      orientations[INST_CNT];

void update_cam_view()
{
  view_calc(&scn.view, config.width, config.height, &scn.cam);

  gpu_write_buf(GLOB, GLOB_BUF_OFS_CAM, &scn.cam, CAM_BUF_SIZE);
  gpu_write_buf(GLOB, GLOB_BUF_OFS_VIEW, &scn.view, sizeof(view));
  
  gathered_smpls = TEMPORAL_WEIGHT * config.spp;
}

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

  update_cam_view();
}

__attribute__((visibility("default")))
void mouse_move(int32_t dx, int32_t dy)
{
#define LOOK_VEL 0.015f
  
  float theta = min(max(acosf(-scn.cam.fwd.y) + (float)dy * LOOK_VEL, 0.01f), 0.99f * PI);
  float phi = fmodf(atan2f(-scn.cam.fwd.z, scn.cam.fwd.x) + PI - (float)dx * LOOK_VEL, 2.0f * PI);
  
  cam_set_dir(&scn.cam, vec3_spherical(theta, phi));
  
  update_cam_view();
}

__attribute__((visibility("default")))
void init(uint32_t width, uint32_t height)
{
  pcg_srand(42u, 303u);

  // Reserve buffer space
  buf_init(8);
  buf_reserve(GLOB, sizeof(char), GLOB_BUF_SIZE);
  buf_reserve(TRI, sizeof(tri), TRI_CNT);
  buf_reserve(TRI_DATA, sizeof(tri_data), TRI_CNT);
  buf_reserve(INDEX, sizeof(uint32_t), TRI_CNT);
  buf_reserve(BVH_NODE, sizeof(bvh_node), 2 * TRI_CNT);
  buf_reserve(TLAS_NODE, sizeof(tlas_node), 2 * INST_CNT + 1);
  buf_reserve(INST, sizeof(inst), INST_CNT);
  buf_reserve(MAT, sizeof(mat), MAT_CNT);

  buf_acquire(GLOB, GLOB_BUF_SIZE);
  
  config = (cfg){ width, height, 3, 5 };
  
  scene_init(&scn, MESH_CNT, INST_CNT, MAT_CNT);
  
  scn.cam = (cam){ .vert_fov = 60.0f, .foc_dist = 3.0f, .foc_angle = 0.0f };
  cam_set(&scn.cam, (vec3){ 0.0f, 0.0f, -7.5f }, (vec3){ 0.0f, 0.0f, 2.0f });

  mesh_read_bin(&scn.meshes[0], teapot);
  bvh_init(&scn.bvhs[0], scn.meshes[0].tri_cnt);
  bvh_build(&scn.bvhs[0], scn.meshes[0].tris, scn.meshes[0].tri_cnt);

  mesh_read_bin(&scn.meshes[1], dragon);
  bvh_init(&scn.bvhs[1], scn.meshes[1].tri_cnt);
  bvh_build(&scn.bvhs[1], scn.meshes[1].tris, scn.meshes[1].tri_cnt);

  /*for(uint32_t j=0; j<MESH_CNT; j++) {
    mesh_init(&scn.meshes[j], TRI_CNT);
    
    for(uint32_t i=0; i<TRI_CNT; i++) {
      vec3 a = vec3_sub(vec3_scale(vec3_rand(), 5.0f), (vec3){ 2.5f, 2.5f, 2.5f });
      scn.meshes[j].tris[i].v0 = a;
      scn.meshes[j].tris[i].v1 = vec3_add(a, vec3_rand());
      scn.meshes[j].tris[i].v2 = vec3_add(a, vec3_rand());
      scn.meshes[j].tris_data[i].n0 = vec3_rand();
      scn.meshes[j].tris_data[i].n1 = vec3_rand();
      scn.meshes[j].tris_data[i].n2 = vec3_rand();
      tri_calc_center(&scn.meshes[j].tris[i]);
    }

    bvh_init(&scn.bvhs[j], scn.meshes[j].tri_cnt);
    bvh_build(&scn.bvhs[j], scn.meshes[j].tris, scn.meshes[j].tri_cnt);
  }*/

  for(uint32_t i=0; i<MAT_CNT; i++)
    mat_rand(&scn.materials[i]);

  for(uint32_t i=0; i<INST_CNT; i++) {
	  positions[i] = vec3_scale(vec3_sub(vec3_rand(), (vec3){ 0.5f, 0.5f, 0.5f }), 4.0f);
	  directions[i] = vec3_scale(vec3_unit(positions[i]), 0.05f);
	  orientations[i] = vec3_scale(vec3_rand(), 2.5f);
  }

  // Create GPU buffer
  gpu_create_res(buf_len(GLOB), buf_len(TRI), buf_len(TRI_DATA), buf_len(INDEX),
      buf_len(BVH_NODE), buf_len(TLAS_NODE), buf_len(INST), buf_len(MAT));

  // Write config into globals
  gpu_write_buf(GLOB, GLOB_BUF_OFS_CFG, &config, sizeof(cfg));

  // Write all static data buffer
  gpu_write_buf(TRI, 0, buf_ptr(TRI, 0), buf_len(TRI));
  gpu_write_buf(TRI_DATA, 0, buf_ptr(TRI_DATA, 0), buf_len(TRI_DATA));
  gpu_write_buf(INDEX, 0, buf_ptr(INDEX, 0), buf_len(INDEX));
  gpu_write_buf(BVH_NODE, 0, buf_ptr(BVH_NODE, 0), buf_len(BVH_NODE));
  gpu_write_buf(MAT, 0, buf_ptr(MAT, 0), buf_len(MAT));

  // Write initial view and cam into globals
  update_cam_view();
}

__attribute__((visibility("default")))
void update(float time)
{
  // Orbit cam
  if(orbit_cam) {
    float s = 0.3f;
    float r = 8.0f;
    float h = 0.0f;
    vec3 pos = (vec3){ r * sinf(time * s), h * sinf(time * s * 0.7f), r * cosf(time * s) };
    cam_set(&scn.cam, pos, vec3_neg(pos));
    update_cam_view();
  }

  // Create/update instances
  for(uint32_t i=0; i<INST_CNT; i++) {
    mat4 transform;
		
    mat4 rotx, roty, rotz;
    mat4_rot_x(rotx, orientations[i].x);
    mat4_rot_y(roty, orientations[i].y);
    mat4_rot_z(rotz, orientations[i].z);
    mat4_mul(transform, rotx, roty);
    mat4_mul(transform, transform, rotz);
     
    mat4 scale;
    mat4_scale(scale, (i % 2 == 1) ? 0.008f : 0.4f);
    //mat4_scale(scale, 0.2f);
    mat4_mul(transform, transform, scale);
    
    mat4 translation;
    mat4_trans(translation, positions[i]);
    mat4_mul(transform, translation, transform);

    inst_create(&scn.instances[i], i, transform,
        &scn.meshes[i % MESH_CNT], &scn.bvhs[i % MESH_CNT],
        LAMBERT, &scn.materials[i % MAT_CNT]);
	
    if(!paused) {
      positions[i] = vec3_add(positions[i], directions[i]);
      orientations[i] = vec3_add(orientations[i], directions[i]);

      if(positions[i].x < -3.0f || positions[i].x > 3.0f)
        directions[i].x *= -1.0f;
      if(positions[i].y < -3.0f || positions[i].y > 3.0f)
        directions[i].y *= -1.0f;
      if(positions[i].z < -3.0f || positions[i].z > 3.0f)
        directions[i].z *= -1.0f;

      gathered_smpls = TEMPORAL_WEIGHT * config.spp;
    }
	}
  
  // Build tlas
  tlas_build(scn.tlas_nodes, scn.instances, INST_CNT);

  // Write tlas and instance buffer
  gpu_write_buf(TLAS_NODE, 0, buf_ptr(TLAS_NODE, 0), buf_len(TLAS_NODE));
  gpu_write_buf(INST, 0, buf_ptr(INST, 0), buf_len(INST));

  // Push frame data
  float frame[8] = { pcg_randf(), config.spp / (float)(gathered_smpls + config.spp),
    time, 0.0f, bg_col.x, bg_col.y, bg_col.z, 0.0f };
  gpu_write_buf(GLOB, GLOB_BUF_OFS_FRAME, frame, 8 * sizeof(float));

  gathered_smpls += config.spp;
}

__attribute__((visibility("default")))
void release()
{
  scene_release(&scn);
  buf_release();
}
