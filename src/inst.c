#include "inst.h"
#include "buf.h"
#include "aabb.h"
#include "mesh.h"
#include "bvh.h"
//#include "log.h"

void inst_create(inst *inst, size_t mesh_idx, size_t inst_idx,
    const mesh *mesh, const bvh *bvh, const mat4 transform,
    size_t mat_type, const mat *mat)
{
  inst->tri_ofs = buf_idx(TRI, mesh->tris);
  inst->bvh_node_ofs = buf_idx(BVH_NODE, bvh->nodes);
  inst->id = (mesh_idx << 20) | (inst_idx & 0xfffff);
  inst->mat_id = (mat_type << 24) | (buf_idx(MAT, mat) & 0xffffff);

  //log("mesh idx: %d, inst idx: %d", mesh_idx, inst_idx);
  //log("tri_ofs: %d, bvh_node_ofs: %d", inst->tri_ofs, inst->bvh_node_ofs);
  
  // Store root node bounds transformed into world space
  aabb a = aabb_init();
  vec3 mi = bvh->nodes[0].min;
  vec3 ma = bvh->nodes[0].max;
  
  aabb_grow(&a, mat4_mul_pos(transform, (vec3){ mi.x, mi.y, mi.z }));
  aabb_grow(&a, mat4_mul_pos(transform, (vec3){ ma.x, mi.y, mi.z }));
  aabb_grow(&a, mat4_mul_pos(transform, (vec3){ mi.x, ma.y, mi.z }));
  aabb_grow(&a, mat4_mul_pos(transform, (vec3){ ma.x, ma.y, mi.z }));
  aabb_grow(&a, mat4_mul_pos(transform, (vec3){ mi.x, mi.y, ma.z }));
  aabb_grow(&a, mat4_mul_pos(transform, (vec3){ ma.x, mi.y, ma.z }));
  aabb_grow(&a, mat4_mul_pos(transform, (vec3){ mi.x, ma.y, ma.z }));
  aabb_grow(&a, mat4_mul_pos(transform, (vec3){ ma.x, ma.y, ma.z }));

  inst->min = a.min;
  inst->max = a.max;

  // Store transform and precalc inverse
  for(size_t i=0; i<16; i++)
    inst->transform[i] = transform[i];

  mat4_inv(inst->inv_transform, transform);
}
