#ifndef ESCENE_H
#define ESCENE_H

#include <stdint.h>

#include "../util/vec3.h"

typedef struct ecam ecam;
typedef struct einst einst;
typedef struct emesh emesh;
typedef struct mtl mtl;

typedef struct escene {
  uint16_t mtl_cnt;
  mtl *mtls;
  uint16_t mesh_cnt;
  emesh *meshes;
  uint16_t inst_cnt;
  einst *instances;
  uint16_t cam_cnt;
  ecam *cams;
  vec3 bg_col;
} escene;

void escene_init(escene *s, uint16_t mesh_cnt, uint16_t mtl_cnt,
                 uint16_t cam_cnt, uint16_t inst_cnt);
void escene_release(escene *s);

uint16_t escene_add_mtl(escene *s, mtl *mtl);
uint16_t escene_add_cam(escene *s, vec3 pos, vec3 dir, float vert_fov);
uint16_t escene_add_inst(escene *s, uint16_t mesh_id, uint16_t flags,
                         vec3 scale, float *rot, vec3 trans);

emesh *escene_acquire_mesh(escene *s);
uint16_t escene_attach_mesh(escene *s, emesh *m);

uint32_t escene_calc_size(const escene *s);

#endif
