#ifndef SCENE_H
#define SCENE_H

#include <stdint.h>
#include "mat4.h"
#include "cam.h"
#include "view.h"
#include "types.h"

typedef struct mtl mtl;
typedef struct mesh mesh;
typedef struct bvh bvh;
typedef struct inst inst;
typedef struct inst_info inst_info;
typedef struct tlas_node tlas_node;
typedef struct ltri ltri;

typedef struct scene {
  uint32_t    mtl_cnt;
  mtl         *mtls;
  uint32_t    mesh_cnt;
  mesh        *meshes;
  uint32_t    bvh_cnt;
  bvh         *bvhs;
  uint32_t    inst_cnt;
  inst        *instances;
  inst_info   *inst_info;
  tlas_node   *tlas_nodes;
  uint32_t    ltri_cnt;
  ltri        *ltris;
  view        view;
  cam         cam;
  uint32_t    dirty;
  uint32_t    curr_ofs;
} scene;

void      scene_init(scene *s, uint32_t mesh_cnt, uint32_t mtl_cnt, uint32_t inst_cnt);
void      scene_release();

void      scene_set_dirty(scene *s, res_type r);
void      scene_clr_dirty(scene *s, res_type r);

void      scene_build_bvhs(scene *s);
void      scene_prepare_ltris(scene *s);
void      scene_prepare_render(scene *s);

uint32_t  scene_add_mtl(scene *s, mtl *mtl);

uint32_t  scene_add_mesh_inst(scene *s, uint32_t mesh_id, int32_t mtl_id, mat4 transform); // mtl_id < 0 means no mtl override
uint32_t  scene_add_shape_inst(scene *s, shape_type shape, uint16_t mtl_id, mat4 transform);

void      scene_upd_inst_trans(scene *s, uint32_t inst_id, mat4 transform);
void      scene_upd_inst_mtl(scene *s, uint32_t inst_id, int32_t mtl_id);

// Mainly for IS_DISABLED state updates for now. Everything else is done via scene_upd*().
void      scene_set_inst_state(scene *s, uint32_t inst_id, uint32_t state);
void      scene_clr_inst_state(scene *s, uint32_t inst_id, uint32_t state);
uint32_t  scene_get_inst_state(scene *s, uint32_t inst_id);

mesh      *scene_acquire_mesh(scene *s);
uint32_t  scene_attach_mesh(scene *s, mesh *m, bool is_mesh_emissive);

#endif
