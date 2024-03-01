#ifndef SCENE_H
#define SCENE_H

#include <stdint.h>
#include "mat4.h"
#include "view.h"
#include "cam.h"

typedef struct mat mat;
typedef struct mesh mesh;
typedef struct bvh bvh;
typedef struct inst inst;
typedef struct inst_state inst_state;
typedef struct tlas_node tlas_node;

typedef struct scene {
  uint32_t    mat_cnt;
  mat         *materials;
  uint32_t    mesh_cnt;
  mesh        *meshes;
  bvh         *bvhs;
  uint32_t    inst_cnt;
  inst        *instances;
  inst_state  *inst_states;
  tlas_node   *tlas_nodes;
  view        view;
  cam         cam;
  bool        meshes_dirty; // TODO Do we want to support that?
  bool        materials_dirty;
  uint32_t    curr_ofs;
} scene;

void      scene_init(scene *scene, uint32_t mesh_cnt, uint32_t mat_cnt, uint32_t inst_cnt);
void      scene_release();

void      scene_build_bvhs(scene *s);

void      scene_prepare_render(scene *s);

uint32_t  scene_add_mat(scene *s, mat *material);
void      scene_upd_mat(scene *s, uint32_t mat_id, mat *material);

uint32_t  scene_add_inst(scene *s, uint32_t mesh_id, uint32_t mat_id, mat4 transform);
void      scene_upd_inst(scene *s, uint32_t inst_id, mat4 transform);

uint32_t  scene_add_quad(scene *s, vec3 pos, vec3 nrm, float w, float h);
uint32_t  scene_add_uvsphere(scene *s, float radius, uint32_t subx, uint32_t suby, bool face_normals);

#endif
