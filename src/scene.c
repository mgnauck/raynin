#include "scene.h"
#include "sutil.h"
#include "buf.h"
#include "aabb.h"
#include "mesh.h"
#include "inst.h"
#include "mat.h"
#include "bvh.h"
#include "tlas.h"

void scene_init(scene *s, uint16_t mesh_cnt, uint16_t inst_cnt, uint16_t mat_cnt)
{
  s->mesh_cnt = mesh_cnt;
  s->inst_cnt = 0;
  s->mat_cnt = 0;

  s->meshes = malloc(mesh_cnt * sizeof(mesh));
  s->instances = buf_acquire(INST, inst_cnt);
  s->materials = buf_acquire(MAT, mat_cnt);
  s->bvhs = malloc(mesh_cnt * sizeof(bvh));
  s->tlas_nodes = buf_acquire(TLAS_NODE, 2 * inst_cnt + 1);
}

uint16_t scene_add_mat(scene *s, mat *material)
{
  scene_upd_mat(s, s->mat_cnt, material);
  return s->mat_cnt++;
}

void scene_upd_mat(scene *s, uint16_t mat_id, mat *material)
{
  memcpy(&s->materials[mat_id], material, sizeof(*s->materials));
}

uint16_t scene_add_inst(scene *s, const bvh *bvh, uint16_t mat_id, mat4 transform)
{
  inst *inst = &s->instances[s->inst_cnt];

  // 65536 materials, 65536 instances
  inst->id = (mat_id << 16) | s->inst_cnt;

  // ofs will be used for tris, indices and bvh nodes
  // offset into bvh->nodes is implicitly 2 * ofs
  inst->ofs = buf_idx(TRI, bvh->mesh->tris);
  
  scene_upd_inst(s, s->inst_cnt, bvh, transform);

  return s->inst_cnt++;
}

void scene_upd_inst(scene *s, uint16_t inst_id, const bvh *bvh, mat4 transform)
{
  inst *inst = &s->instances[inst_id];
  
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

void scene_release(scene *s)
{
  for(uint32_t i=0; i<s->mesh_cnt; i++)
    mesh_release(&s->meshes[i]);
  free(s->meshes);
  free(s->bvhs);
}
