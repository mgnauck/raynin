#ifndef INTERSECT_H
#define INTERSECT_H

#include <float.h>
#include <stdint.h>
#include "../util/vec3.h"

#define MAX_DISTANCE  FLT_MAX

typedef struct inst inst;
typedef struct mesh mesh;
typedef struct node node;
typedef struct ray ray;
typedef struct tri tri;

typedef struct hit {
  float     t;
  float     u;
  float     v;
  uint32_t  e;  // mesh type instance: (tri id << 16) | (inst id & 0xffff)
} hit;          // shape type instance: (shape type << 16) | (inst id & 0xffff)

float intersect_aabb(const ray *r, float curr_dist, vec3 min_ext, vec3 max_ext);
void  intersect_plane(const ray *r, uint32_t inst_id, hit *h);
void  intersect_unitbox(const ray *r, uint32_t inst_id, hit *h);
void  intersect_unitsphere(const ray *r, uint32_t inst_id, hit *h);
void  intersect_tri(const ray *r, const tri *t, uint32_t inst_id, uint32_t tri_id, hit *h);
void  intersect_blas(const ray *r, const node *blas_nodes, const tri *tris, uint32_t inst_id, hit *h);
void  intersect_inst(const ray *r, const inst *b, const mesh *meshes, const node *blas_nodes, hit *h);
void  intersect_tlas(const ray *r, const node *tlas_nodes, const inst *instances, const mesh *meshes, const node *blas_nodes, hit *h);

#endif
