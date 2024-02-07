#ifndef SCENE_H
#define SCENE_H

#include <stdint.h>
#include "view.h"
#include "cam.h"

typedef struct mesh mesh;
typedef struct inst inst;
typedef struct mat mat;
typedef struct bvh bvh;
typedef struct tlas_node tlas_node;

typedef struct scene {
  uint32_t    mesh_cnt;
  mesh        *meshes;
  uint32_t    inst_cnt;
  inst        *instances;
  uint32_t    mat_cnt;
  mat         *materials;
  bvh         *bvhs;
  tlas_node   *tlas_nodes;
  view        view;
  cam         cam;
} scene;

void scene_init(scene *scene, uint32_t mesh_cnt, uint32_t inst_cnt, uint32_t mat_cnt);
void scene_release();

#endif
