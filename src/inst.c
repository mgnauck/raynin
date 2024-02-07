#include "inst.h"
#include "buf.h"
#include "aabb.h"
#include "mesh.h"
#include "bvh.h"

void inst_create(inst *inst, uint32_t inst_idx, const mat4 transform,
    const mesh *mesh, const bvh *bvh, uint32_t mat_type, const mat *mat)
{
  // 16 material types, 4096 materials, 65536 instances
  inst->id = (mat_type << 28) | ((buf_idx(MAT, mat) & 0xfff) << 16) | (inst_idx & 0xffff);

  // ofs will be used for tris, trisData, indices and bvh nodes
  // offset into bvh->nodes is implicitly 2*ofs
  inst->ofs = buf_idx(TRI, mesh->tris);

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
