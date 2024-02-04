#ifndef SCENE_H
#define SCENE_H

#include <stddef.h>
#include "view.h"
#include "cam.h"

typedef struct mesh mesh;
typedef struct inst inst;
typedef struct mat mat;
typedef struct bvh bvh;
typedef struct tlas_node tlas_node;

typedef struct scene {
  size_t      mesh_cnt;
  mesh        *meshes;
  size_t      inst_cnt;
  inst        *instances;
  size_t      mat_cnt;
  mat         *materials;
  bvh         *bvhs;
  tlas_node   *tlas_nodes;
  view        view;
  cam         cam;
} scene;

void scene_init(scene *scene, size_t mesh_cnt, size_t inst_cnt, size_t mat_cnt);
void scene_release();

#endif
