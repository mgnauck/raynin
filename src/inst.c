#include "inst.h"
#include "buf.h"
#include "aabb.h"
#include "mesh.h"
#include "bvh.h"

void inst_create(inst *inst, uint32_t inst_idx, const mat4 transform,
    const bvh *bvh, const mat *mat)
{
  // 65536 materials, 65536 instances
  inst->id = (buf_idx(MAT, mat) << 16) | (inst_idx & 0xffff);

  // ofs will be used for tris, indices and bvh nodes
  // offset into bvh->nodes is implicitly 2 * ofs
  inst->ofs = buf_idx(TRI, bvh->mesh->tris);

  inst_transform(inst, bvh, transform);
}

void inst_transform(inst *inst, const bvh *bvh, const mat4 transform)
{
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
  for(uint8_t i=0; i<16; i++)
    inst->transform[i] = transform[i];

  mat4_inv(inst->inv_transform, transform);
}
