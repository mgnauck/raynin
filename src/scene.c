#include "scene.h"
#include "sutil.h"
#include "mutil.h"
#include "tri.h"
#include "aabb.h"
#include "mtl.h"
#include "mesh.h"
#include "bvh.h"
#include "inst.h"
#include "tlas.h"

typedef struct inst_state {
  uint32_t  mesh_id;
  bool      dirty;
} inst_state;

void scene_init(scene *s, uint32_t mesh_cnt, uint32_t mtl_cnt, uint32_t inst_cnt)
{
  s->mtls         = malloc(mtl_cnt * sizeof(*s->mtls)); 
  s->meshes       = malloc(mesh_cnt * sizeof(*s->meshes));
  s->bvhs         = malloc(mesh_cnt * sizeof(bvh));
  s->instances    = malloc(inst_cnt * sizeof(*s->instances));
  s->inst_states  = malloc(inst_cnt * sizeof(*s->inst_states));
  s->tlas_nodes   = malloc((2 * inst_cnt + 1) * sizeof(*s->tlas_nodes)); // TODO Switch to 2 * inst_cnt only

  s->mtl_cnt  = 0;
  s->mesh_cnt = 0;
  s->inst_cnt = 0;
  s->curr_ofs = 0;

  scene_set_dirty(s, RT_CAM_VIEW | RT_MESH | RT_MTL | RT_INST);
}

void scene_release(scene *s)
{
  for(uint32_t i=0; i<s->mesh_cnt; i++)
    mesh_release(&s->meshes[i]);

  free(s->tlas_nodes);
  free(s->inst_states);
  free(s->instances);
  free(s->bvhs);
  free(s->meshes);
  free(s->mtls);
}

void scene_set_dirty(scene *s, res_type r)
{
  s->dirty |= r;
}

void scene_unset_dirty(scene *s, res_type r)
{
  s->dirty &= ~r;
}

void scene_build_bvhs(scene *s)
{
  for(uint32_t i=0; i<s->mesh_cnt; i++) {
    bvh_init(&s->bvhs[i], &s->meshes[i]);
    bvh_build(&s->bvhs[i]);
  }
}

void scene_prepare_render(scene *s)
{
  // Update instances
  bool rebuild_tlas = false;
  for(uint32_t i=0; i<s->inst_cnt; i++) {
    inst_state *state = &s->inst_states[i]; 
    if(state->dirty) {
      inst *inst = &s->instances[i];
      
      // Calc inverse transform
      mat4 *t = &inst->transform;
      mat4_inv(inst->inv_transform, *t);
  
      // Store root node bounds transformed into world space
      aabb a = aabb_init();
      vec3 mi = s->bvhs[state->mesh_id].nodes[0].min;
      vec3 ma = s->bvhs[state->mesh_id].nodes[0].max;

      aabb_grow(&a, mat4_mul_pos(*t, (vec3){ mi.x, mi.y, mi.z }));
      aabb_grow(&a, mat4_mul_pos(*t, (vec3){ ma.x, mi.y, mi.z }));
      aabb_grow(&a, mat4_mul_pos(*t, (vec3){ mi.x, ma.y, mi.z }));
      aabb_grow(&a, mat4_mul_pos(*t, (vec3){ ma.x, ma.y, mi.z }));
      aabb_grow(&a, mat4_mul_pos(*t, (vec3){ mi.x, mi.y, ma.z }));
      aabb_grow(&a, mat4_mul_pos(*t, (vec3){ ma.x, mi.y, ma.z }));
      aabb_grow(&a, mat4_mul_pos(*t, (vec3){ mi.x, ma.y, ma.z }));
      aabb_grow(&a, mat4_mul_pos(*t, (vec3){ ma.x, ma.y, ma.z }));

      inst->min = a.min;
      inst->max = a.max;

      state->dirty = false;
      rebuild_tlas = true;
    }
  }

  if(rebuild_tlas) {
    tlas_build(s->tlas_nodes, s->instances, s->inst_cnt);
    scene_set_dirty(s, RT_INST);
  }
}

uint32_t scene_add_mtl(scene *s, mtl *mtl)
{
  scene_upd_mtl(s, s->mtl_cnt, mtl);
  return s->mtl_cnt++;
}

void scene_upd_mtl(scene *s, uint32_t mtl_id, mtl *mtl)
{
  memcpy(&s->mtls[mtl_id], mtl, sizeof(*s->mtls));
  scene_set_dirty(s, RT_MTL);
}

uint32_t scene_add_inst(scene *s, uint32_t mesh_id, uint32_t mtl_id, mat4 transform)
{
  inst_state *inst_state = &s->inst_states[s->inst_cnt];
  inst_state->mesh_id = mesh_id;
  inst_state->dirty = true;

  inst *inst = &s->instances[s->inst_cnt];
  inst->id = (mtl_id << 16) | s->inst_cnt;
  inst->ofs = s->meshes[s->inst_cnt].ofs;

  scene_upd_inst(s, s->inst_cnt, transform);

  return s->inst_cnt++;
}

void scene_upd_inst(scene *s, uint32_t inst_id, mat4 transform)
{
  memcpy(s->instances[inst_id].transform, transform, sizeof(mat4));
  s->inst_states[inst_id].dirty = true;
}

void update_ofs(scene *s, mesh *m)
{
  m->ofs = s->curr_ofs * sizeof(tri);
  s->curr_ofs += m->tri_cnt;
}

uint32_t scene_add_quad(scene *s, vec3 pos, vec3 nrm, float w, float h)
{
  mesh *m = &s->meshes[s->mesh_cnt];
  mesh_init(m, 2);
  
  nrm = vec3_unit(nrm);
  
  vec3 r = (fabsf(nrm.x) > 0.9) ? (vec3){ 0.0, 1.0, 0.0 } : (vec3){ 0.0, 0.0, 1.0 };
  vec3 t = vec3_cross(nrm, r);
  vec3 b = vec3_cross(t, nrm);

  t = vec3_scale(t, 0.5f * w);
  b = vec3_scale(b, 0.5f * h);

  tri *tri = &m->tris[0];
  tri->v0 = vec3_sub(vec3_sub(pos, t), b);
  tri->v1 = vec3_add(vec3_sub(pos, t), b);
  tri->v2 = vec3_add(vec3_add(pos, t), b);
  tri->n0 = tri->n1 = tri->n2 = nrm;
#ifdef TEXTURE_SUPPORT // Untested
  tri->uv0[0] = 0.0f; tri->uv0[1] = 0.0f;
  tri->uv1[0] = 0.0f; tri->uv1[1] = 1.0f;
  tri->uv2[0] = 1.0f; tri->uv2[1] = 1.0f;
#endif
  m->centers[0] = tri_calc_center(tri);

  tri = &m->tris[1];
  tri->v0 = vec3_sub(vec3_sub(pos, t), b);
  tri->v1 = vec3_add(vec3_add(pos, t), b);
  tri->v2 = vec3_sub(vec3_add(pos, t), b);
  tri->n0 = tri->n1 = tri->n2 = nrm;
#ifdef TEXTURE_SUPPORT // Untested
  tri->uv0[0] = 0.0f; tri->uv0[1] = 0.0f;
  tri->uv1[0] = 1.0f; tri->uv1[1] = 1.0f;
  tri->uv2[0] = 1.0f; tri->uv2[1] = 0.0f;
#endif
  m->centers[1] = tri_calc_center(tri);

  update_ofs(s, m);
  return s->mesh_cnt++;
}

uint32_t scene_add_uvsphere(scene *s, float radius, uint32_t subx, uint32_t suby, bool face_normals)
{
  mesh *m = &s->meshes[s->mesh_cnt];
  mesh_init(m, 2 * subx * suby);

  float dphi = 2 * PI / subx;
  float dtheta = PI / suby;
  float inv_r = 1.0f / radius;

  float theta = 0.0f;
  for(uint32_t j=0; j<suby; j++) {
    float phi = 0.0f;
    for(uint32_t i=0; i<subx; i++) {
      uint32_t ofs = 2 * (subx * j + i);

      vec3 a = vec3_scale(vec3_spherical(theta + dtheta, phi), radius);
      vec3 b = vec3_scale(vec3_spherical(theta, phi), radius);
      vec3 c = vec3_scale(vec3_spherical(theta, phi + dphi), radius);
      vec3 d = vec3_scale(vec3_spherical(theta + dtheta, phi + dphi), radius);

      tri *t1 = &m->tris[ofs];
      *t1 = (tri){ .v0 = a, .v1 = b, .v2 = c,
#ifdef TEXTURE_SUPPORT // Untested
        .uv0[0] = phi / 2.0f * PI, .uv0[1] = (theta + dtheta) / PI,
        .uv1[0] = phi / 2.0f * PI, .uv1[1] = theta / PI,
        .uv2[0] = (phi + dphi) / 2.0f * PI, .uv2[1] = theta / PI,
#endif
      };
      
      m->centers[ofs] = tri_calc_center(t1);
      
      if(face_normals) {
        t1->n0 = vec3_unit(vec3_cross(vec3_sub(a, b), vec3_sub(c, b)));
        t1->n1 = t1->n0;
        t1->n2 = t1->n0;
      } else {
        t1->n0 = vec3_scale(a, inv_r);
        t1->n1 = vec3_scale(b, inv_r);
        t1->n2 = vec3_scale(c, inv_r);
      }

      tri *t2 = &m->tris[ofs + 1];
      *t2 = (tri){ .v0 = a, .v1 = c, .v2 = d,
#ifdef TEXTURE_SUPPORT // Untested
        .uv0[0] = t1->uv0[0], .uv0[1] = t1->uv0[1],
        .uv1[0] = t1->uv2[0], .uv1[1] = t1->uv1[1],
        .uv2[0] = t1->uv2[0], .uv2[1] = t1->uv0[1],
#endif
      };
      
      m->centers[ofs + 1] = tri_calc_center(t2);
      
      if(face_normals) {
        t2->n0 = vec3_unit(vec3_cross(vec3_sub(a, c), vec3_sub(d, c)));
        t2->n1 = t2->n0;
        t2->n2 = t2->n0;
      } else {
        t2->n0 = vec3_scale(a, inv_r);
        t2->n1 = vec3_scale(c, inv_r);
        t2->n2 = vec3_scale(d, inv_r);
      }

      phi += dphi;
    }

    theta += dtheta;
  }

  update_ofs(s, m);
  return s->mesh_cnt++;
}
