#ifndef INTERSECT_H
#define INTERSECT_H

#include <float.h>
#include <stdint.h>
#include "vec3.h"

#define MAX_DISTANCE  FLT_MAX

typedef struct ray ray;
typedef struct tri tri;
typedef struct bvh_node bvh_node;
typedef struct bvh bvh;
typedef struct tlas_node tlas_node;
typedef struct inst inst;

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
void  intersect_bvh(const ray *r, const bvh_node *nodes, const uint32_t *indices, const tri *tris, uint32_t inst_id, hit *h);
void  intersect_inst(const ray *r, const inst *b, const bvh *bvh, hit *h);
void  intersect_tlas(const ray *r, const tlas_node *nodes, const inst *instances, const bvh *bvhs, hit *h);

#endif
