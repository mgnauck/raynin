#ifndef SCENE_H
#define SCENE_H

#include <stdint.h>
#include "mat4.h"
#include "view.h"
#include "cam.h"

typedef struct mesh mesh;
typedef struct inst inst;
typedef struct mat mat;
typedef struct bvh bvh;
typedef struct tlas_node tlas_node;

typedef struct scene {
  uint16_t    mesh_cnt;
  mesh        *meshes;
  uint16_t    inst_cnt;
  inst        *instances;
  uint16_t    mat_cnt;
  mat         *materials;
  bvh         *bvhs;
  tlas_node   *tlas_nodes;
  view        view;
  cam         cam;
} scene;

void      scene_init(scene *scene, uint16_t mesh_cnt, uint16_t inst_cnt, uint16_t mat_cnt);

uint16_t  scene_add_mat(scene *s, mat *material);
void      scene_upd_mat(scene *s, uint16_t mat_id, mat *material);

uint16_t  scene_add_inst(scene *s, const bvh *b, uint16_t mat_id, mat4 transform);
void      scene_upd_inst(scene *s, uint16_t inst_id, const bvh *bvh, mat4 transform);

void      scene_release();

#endif
