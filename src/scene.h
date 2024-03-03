#ifndef SCENE_H
#define SCENE_H

#include <stdint.h>
#include "mat4.h"
#include "cam.h"
#include "view.h"

typedef enum res_type {
  RT_CAM_VIEW  = 1,
  RT_MESH      = 2,
  RT_MTL       = 4,
  RT_INST      = 8
} res_type;

typedef struct mtl mtl;
typedef struct mesh mesh;
typedef struct bvh bvh;
typedef struct inst inst;
typedef struct inst_state inst_state;
typedef struct tlas_node tlas_node;

typedef struct scene {
  uint32_t    mtl_cnt;
  mtl         *mtls;
  uint32_t    mesh_cnt;
  mesh        *meshes;
  bvh         *bvhs;
  uint32_t    inst_cnt;
  inst        *instances;
  inst_state  *inst_states;
  tlas_node   *tlas_nodes;
  view        view;
  cam         cam;
  uint32_t    dirty;
  uint32_t    curr_ofs;
} scene;

void      scene_init(scene *scene, uint32_t mesh_cnt, uint32_t mtl_cnt, uint32_t inst_cnt);
void      scene_release();

void      scene_set_dirty(scene *s, res_type r);
void      scene_unset_dirty(scene *s, res_type r);

void      scene_build_bvhs(scene *s);
void      scene_prepare_render(scene *s);

uint32_t  scene_add_mtl(scene *s, mtl *mtl);
void      scene_upd_mtl(scene *s, uint32_t mtl_id, mtl *mtl);

uint32_t  scene_add_inst(scene *s, uint32_t mesh_id, uint32_t mtl_id, mat4 transform);
void      scene_upd_inst(scene *s, uint32_t inst_id, mat4 transform);

uint32_t  scene_add_quad(scene *s, vec3 pos, vec3 nrm, float w, float h);
uint32_t  scene_add_uvsphere(scene *s, float radius, uint32_t subx, uint32_t suby, bool face_normals);

#endif
