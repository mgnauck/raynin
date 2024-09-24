#ifndef EX_SCENE_H
#define EX_SCENE_H

#include <stdint.h>
#include "../util/vec3.h"

typedef struct ex_cam ex_cam;
typedef struct ex_inst ex_inst;
typedef struct ex_mesh ex_mesh;
typedef struct mtl mtl;

typedef struct ex_scene {
  uint32_t    mtl_cnt;
  mtl         *mtls;
  uint32_t    mesh_cnt;
  ex_mesh     *meshes;
  uint32_t    inst_cnt;
  ex_inst     *instances;
  uint32_t    cam_cnt;
  ex_cam      *cams;
  vec3        bg_col;
} ex_scene;

void      ex_scene_init(ex_scene *s, uint16_t mesh_cnt, uint16_t mtl_cnt, uint16_t cam_cnt, uint16_t inst_cnt);
void      ex_scene_release(ex_scene *s);

uint16_t  ex_scene_add_mtl(ex_scene *s, mtl *mtl);
uint16_t  ex_scene_add_cam(ex_scene *s, vec3 eye, vec3 tgt, float vert_fov);
uint16_t  ex_scene_add_inst(ex_scene *s, uint16_t mesh_id, vec3 scale, float *rot, vec3 trans);

ex_mesh   *ex_scene_acquire_mesh(ex_scene *s);
uint16_t  ex_scene_attach_mesh(ex_scene *s, ex_mesh *m);

#endif
