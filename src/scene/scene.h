#ifndef SCENE_H
#define SCENE_H

#include <stdint.h>

#include "../util/mat4.h"
#include "types.h"

#define NO_MTL_OVERRIDE 0xffff

typedef struct cam cam;
typedef struct inst inst;
typedef struct inst_info inst_info;
typedef struct ltri ltri;
typedef struct mesh mesh;
typedef struct mtl mtl;
typedef struct node node;

typedef struct scene {
  uint16_t max_mtl_cnt;
  uint16_t mtl_cnt;
  mtl *mtls;
  uint16_t max_mesh_cnt;
  uint16_t mesh_cnt;
  mesh *meshes;
  uint16_t max_inst_cnt;
  uint16_t inst_cnt;
  inst *instances;
  inst_info *inst_info;
  node *tlas_nodes;
  uint32_t max_ltri_cnt;
  uint32_t ltri_cnt;
  ltri *ltris;
  uint32_t *tri_ids;
  uint32_t max_tri_cnt;
  node *blas_nodes;
  cam *cams;
  uint16_t cam_cnt;
  cam *active_cam;
  vec3 bg_col;
  uint32_t dirty;
  uint32_t curr_ofs;
} scene;

void scene_init(scene *s, uint16_t mesh_cnt, uint16_t mtl_cnt, uint16_t cam_cnt,
                uint16_t inst_cnt);
void scene_release(scene *s);

void scene_set_dirty(scene *s, res_type r);
void scene_clr_dirty(scene *s, res_type r);

void scene_finalize(scene *s); // Call after loading the scene data
void scene_prepare_render(
    scene *s); // Call after each scene data update before render

uint16_t scene_add_mtl(scene *s, mtl *mtl);

void scene_set_active_cam(scene *s, uint16_t cam_id);
cam *scene_get_active_cam(scene *s);
cam *scene_get_cam(scene *s, uint16_t cam_id);

uint16_t
scene_add_inst(scene *s, uint16_t mesh_id, uint16_t mtl_id, uint32_t flags,
               mat4 transform); // mtl_id == 0xffff means no mtl override

void scene_upd_inst_trans(scene *s, uint16_t inst_id, mat4 transform);
void scene_upd_inst_mtl(scene *s, uint16_t inst_id, uint16_t mtl_id);
void scene_upd_inst_flags(scene *s, uint16_t inst_id, uint32_t flags);

// Mainly for IS_DISABLED state updates for now. Everything else is done via
// scene_upd*().
void scene_set_inst_state(scene *s, uint16_t inst_id, uint32_t state);
void scene_clr_inst_state(scene *s, uint16_t inst_id, uint32_t state);
uint32_t scene_get_inst_state(scene *s, uint16_t inst_id);

mesh *scene_acquire_mesh(scene *s); // Pair calls to acquire and attach, i.e.
                                    // can not acquire twice in a row
uint16_t scene_attach_mesh(scene *s, mesh *m, bool is_mesh_emissive);

#endif
