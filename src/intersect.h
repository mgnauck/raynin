#ifndef INTERSECT_H
#define INTERSECT_H

#include <float.h>
#include <stddef.h>
#include "vec3.h"

#define MAX_DISTANCE  FLT_MAX

typedef struct ray ray;
typedef struct tri tri;
typedef struct bvh_node bvh_node;
typedef struct tlas_node tlas_node;
typedef struct inst inst;

typedef struct hit {
  float   t;
  float   u;
  float   v;
  size_t  id; // (tri id << 16) | (inst id & 0xffff)
} hit;

float intersect_aabb(const ray *r, float curr_dist, vec3 min_ext, vec3 max_ext);
void  intersect_tri(const ray *r, const tri *t, size_t inst_id, size_t tri_id, hit *h);
void  intersect_bvh(const ray *r, const bvh_node *nodes, const size_t *indices, const tri *tris, size_t inst_id, hit *h);
void  intersect_inst(const ray *r, const inst *b, hit *h);
void  intersect_tlas(const ray *r, const tlas_node *nodes, const inst *instances, hit *h);

#endif
