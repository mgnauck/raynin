#include "scene.h"
#include "sutil.h"
#include "mutil.h"
#include "tri.h"
#include "aabb.h"
#include "mtl.h"
#include "mesh.h"
#include "bvh.h"
#include "inst.h"
#include "settings.h"
#include "tlas.h"
#include "log.h"

void scene_init(scene *s, uint32_t mesh_cnt, uint32_t mtl_cnt, uint32_t inst_cnt, uint32_t ltri_cnt)
{
  s->mtls         = malloc(mtl_cnt * sizeof(*s->mtls)); 
  s->meshes       = malloc(mesh_cnt * sizeof(*s->meshes));
  s->bvhs         = malloc(mesh_cnt * sizeof(bvh));
  s->instances    = malloc(inst_cnt * sizeof(*s->instances));
  s->inst_info    = malloc(inst_cnt * sizeof(*s->inst_info));
  s->tlas_nodes   = malloc(2 * inst_cnt * sizeof(*s->tlas_nodes));
  s->ltris        = malloc(ltri_cnt * sizeof(*s->ltris));

  s->mtl_cnt  = 0;
  s->mesh_cnt = 0;
  s->bvh_cnt  = 0;
  s->inst_cnt = 0;
  s->ltri_cnt = 0;
  s->curr_ofs = 0;

  scene_set_dirty(s, RT_CAM_VIEW);
}

void scene_release(scene *s)
{
  for(uint32_t i=0; i<s->mesh_cnt; i++)
    mesh_release(&s->meshes[i]);

  free(s->tlas_nodes);
  free(s->ltris);
  free(s->inst_info);
  free(s->instances);
  free(s->bvhs);
  free(s->meshes);
  free(s->mtls);
}

void scene_set_dirty(scene *s, res_type r)
{
  s->dirty |= r;
}

void scene_clr_dirty(scene *s, res_type r)
{
  s->dirty &= ~r;
}

void scene_build_bvhs(scene *s)
{
  if(s->dirty & RT_MESH) {
    for(uint32_t i=0; i<s->mesh_cnt; i++) {
      if(s->bvh_cnt == i) {
        bvh_init(&s->bvhs[i], &s->meshes[i]);
        s->bvh_cnt++;
      }
      bvh_build(&s->bvhs[i]);
    }
  }
}

void build_ltris(scene *s, inst *inst, inst_info *info, uint32_t ltri_ofs)
{
  mesh *m = &s->meshes[info->mesh_shape];
  uint32_t tri_cnt = m->tri_cnt;
  tri *tris = m->tris;
  uint32_t ltri_cnt = 0;

  if(inst->data & MTL_OVERRIDE_BIT) {
    // Material override applies to all tris, create ltri for each of them
    for(uint32_t i=0; i<tri_cnt; i++) {
      tri *t = &tris[i];
      uint32_t ltri_id = ltri_ofs + ltri_cnt++;
      tri_build_ltri(&s->ltris[ltri_id], &tris[i],
          inst->id & 0xffff, i, inst->transform,
          s->mtls[t->mtl & 0xffff].col);
      // A tri links its ltri: Tris that emit light need to be unique, i.e.
      // multiple instances of the same light mesh/tri are not allowed :(
      t->ltri_id = ltri_id;
    }
  } else {
    // Create ltris for emissive tris of the mesh only
    for(uint32_t i=0; i<tri_cnt; i++) {
      tri *t = &tris[i];
      mtl *mtl = &s->mtls[t->mtl & 0xffff];
      if(mtl_is_emissive(mtl)) {
        uint32_t ltri_id = ltri_ofs + ltri_cnt++;
        tri_build_ltri(&s->ltris[ltri_id], t,
            inst->id & 0xffff, i, inst->transform, mtl->col);
        t->ltri_id = ltri_id;
      }
    }
  }

  // Remember for future updates
  info->ltri_ofs = ltri_ofs;
  info->ltri_cnt = ltri_cnt;

  scene_set_dirty(s, RT_LTRI);
}

void update_ltris(scene *s, inst *inst, inst_info *info)
{
  tri *tris = s->meshes[info->mesh_shape].tris;
  for(uint32_t i=0; i<info->ltri_cnt; i++) {
    ltri *lt = &s->ltris[info->ltri_ofs + i];
    tri_update_ltri(lt, &tris[lt->tri_id], inst->transform);
  }

  scene_set_dirty(s, RT_LTRI);
}

void scene_prepare_render(scene *s)
{
  ///uint64_t start = SDL_GetTicks64();

  // Update instances
  bool rebuild_tlas = false;
  bool rebuild_ltris = false;
  uint32_t ltri_cnt = 0;
  for(uint32_t i=0; i<s->inst_cnt; i++) {
    inst_info *info = &s->inst_info[i];
    bool disabled = info->state & IS_DISABLED;
    bool emissive = info->state & IS_EMISSIVE; 
    
    // Reset ltri ofs/cnt for disabled instances
    if(disabled)
      info->ltri_ofs = info->ltri_cnt = 0;
    
    // Start rebuilding all ltris once we hit the first dirty emissive or
    // previously emissive instance
    if(!rebuild_ltris && (info->state & IS_MTL_DIRTY) &&
        (emissive || (info->state & IS_WAS_EMISSIVE)))
      rebuild_ltris = true;

    // Check if we are rebuilding ltris 
    if(rebuild_ltris && emissive && !disabled)
      build_ltris(s, &s->instances[i], info, ltri_cnt);

    if(info->state & IS_TRANS_DIRTY) {
      if(!disabled) {
        inst *inst = &s->instances[i];
 
        // Update transformation of existing ltris for this instance
        if(!rebuild_ltris && (info->state & IS_EMISSIVE))
          update_ltris(s, inst, info);

        // Calc inverse transform
        mat4 transform, inv_transform;
        mat4_from_row3x4(transform, inst->transform);
        mat4_inv(inv_transform, transform);
        memcpy(inst->inv_transform, inv_transform, 12 * sizeof(float));
    
        aabb a = aabb_init();
        vec3 mi, ma;

        if(inst->data & SHAPE_TYPE_BIT) {
          // Shape type
          if((inst->data & MESH_SHAPE_MASK) != ST_PLANE) {
            // Box and sphere are of unit size
            mi = (vec3){ -1.0f, -1.0f, -1.0f };
            ma = (vec3){  1.0f,  1.0f,  1.0f };
          } else {
            // Plane is in XZ and has a default size
            float P = 0.5f * PLANE_DEFAULT_SIZE;
            mi = (vec3){ -P, -EPSILON, -P };
            ma = (vec3){  P,  EPSILON,  P };
          }
        } else {
          // Mesh type, use (object space) BVH root nodes
          mi = s->bvhs[info->mesh_shape].nodes[0].min;
          ma = s->bvhs[info->mesh_shape].nodes[0].max;
        }

        // Transform instance aabb to world space
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
      }

      info->state &= ~IS_TRANS_DIRTY;
      rebuild_tlas = true;
    }

    info->state &= ~(IS_MTL_DIRTY | IS_WAS_EMISSIVE);
    ltri_cnt += info->ltri_cnt;
  }

  s->ltri_cnt = ltri_cnt;

  ///uint64_t inst_upd_end = SDL_GetTicks64();

  if(rebuild_tlas) {
    tlas_build(s->tlas_nodes, s->instances, s->inst_info, s->inst_cnt);
    scene_set_dirty(s, RT_INST);
  }

  ///uint64_t tlas_upd_end = SDL_GetTicks64();

  ///logc("Inst update took: %ld", inst_upd_end - start);
  ///logc("Tlas update took: %ld", tlas_upd_end - inst_upd_end);
  ///logc("Full update took: %ld", tlas_upd_end - start);

  logc("Scene prepare");
}

uint32_t scene_add_mtl(scene *s, mtl *mtl)
{
  scene_upd_mtl(s, s->mtl_cnt, mtl);
  return s->mtl_cnt++;
}

// TODO Material updates can trigger light tri changes :(
void scene_upd_mtl(scene *s, uint32_t mtl_id, mtl *mtl)
{
  memcpy(&s->mtls[mtl_id], mtl, sizeof(*s->mtls));
  scene_set_dirty(s, RT_MTL);
}

uint32_t add_inst(scene *s, uint32_t mesh_shape, int32_t mtl_id, mat4 transform)
{
  inst_info *info = &s->inst_info[s->inst_cnt];
  info->mesh_shape = mesh_shape;
  info->state = IS_TRANS_DIRTY | IS_MTL_DIRTY;
  info->ltri_ofs = 0;
  info->ltri_cnt = 0;

  inst *inst = &s->instances[s->inst_cnt];
  // Lowest 16 bits are instance id, i.e. max 65536 instances
  inst->id = s->inst_cnt & INST_ID_MASK;

#ifndef NATIVE_BUILD
  // Lowest 30 bits are shape type (if bit 31 is set) or
  // offset into tris/indices and 2 * data into bvhs
  // i.e. max offset is triangle 1073741823 :)
  inst->data = (mesh_shape & SHAPE_TYPE_BIT) ? mesh_shape : s->meshes[mesh_shape].ofs;
#else
  // For native build, lowest 30 bits are simply the shape type or mesh id
  inst->data = mesh_shape;
#endif

  // Set transform and mtl override id to instance
  scene_upd_inst_trans(s, s->inst_cnt, transform);
  scene_upd_inst_mtl(s, s->inst_cnt, mtl_id);

  return s->inst_cnt++;
}

uint32_t scene_add_inst_mesh(scene *s, uint32_t mesh_id, int32_t mtl_id, mat4 transform)
{
  // Bit 30 not set indicates mesh type
  return add_inst(s, mesh_id & MESH_SHAPE_MASK, mtl_id, transform);
}

uint32_t scene_add_inst_shape(scene *s, shape_type shape, uint16_t mtl_id, mat4 transform)
{
  // Bit 30 set indicates shape type
  // Shape types are always using the material override
  return add_inst(s, SHAPE_TYPE_BIT | (shape & MESH_SHAPE_MASK), mtl_id, transform);
}

void scene_upd_inst_trans(scene *s, uint32_t inst_id, mat4 transform)
{
  memcpy(s->instances[inst_id].transform, transform, 12 * sizeof(float));
  s->inst_info[inst_id].state |= IS_TRANS_DIRTY;
}

void scene_upd_inst_mtl(scene *s, uint32_t inst_id, int32_t mtl_id)
{
  inst *inst = &s->instances[inst_id];
  inst_info *info = &s->inst_info[inst_id];

  if(info->state & IS_EMISSIVE)
    info->state |= IS_WAS_EMISSIVE;

  // Mtl override if mtl_id >= 0
  if(mtl_id >= 0) {
    // Highest 16 bits are mtl override id, i.e. max 65536 materials
    inst->id = (mtl_id << 16) | (inst->id & INST_ID_MASK);
    
    // Set highest bit to enable the material override
    inst->data |= MTL_OVERRIDE_BIT;

    // Flag mesh instance if override material is emissive
    if(!(inst->data & SHAPE_TYPE_BIT)) {
      if(mtl_is_emissive(&s->mtls[mtl_id]))
        info->state |= IS_EMISSIVE;
      else
        info->state &= ~IS_EMISSIVE;
    }
  }
  // Reset mtl override (for mesh types only)
  else if(!(inst->data & SHAPE_TYPE_BIT)) {
    inst->data = inst->data & INST_DATA_MASK;
    
    // Flag instance if mesh is emissive
    if(s->meshes[info->mesh_shape].is_emissive)
      info->state |= IS_EMISSIVE;
    else
      info->state &= ~IS_EMISSIVE;
  }

  s->inst_info[inst_id].state |= IS_MTL_DIRTY;
}

void scene_set_inst_state(scene *s, uint32_t inst_id, uint32_t state)
{
  // Set new state and trigger transformation and material updates
  s->inst_info[inst_id].state |= state | IS_TRANS_DIRTY | IS_MTL_DIRTY;

  ///logc("Set inst state for %d", inst_id);
}

void scene_clr_inst_state(scene *s, uint32_t inst_id, uint32_t state)
{
  // Set new state and trigger transformation and material updates
  s->inst_info[inst_id].state =
    (s->inst_info[inst_id].state & ~state) | IS_TRANS_DIRTY | IS_MTL_DIRTY;

  ///logc("Cleared inst state for %d", inst_id);
}

uint32_t scene_get_inst_state(scene *s, uint32_t inst_id)
{
  return s->inst_info[inst_id].state;
}

void finalize_mesh(scene *s, mesh *m, uint32_t mtl_id)
{
  // Set and update offset into tri buffer
  m->ofs = s->curr_ofs;
  s->curr_ofs += m->tri_cnt;

  // Set flag for emissive material
  m->is_emissive = mtl_is_emissive(&s->mtls[mtl_id]);

  // Flag to trigger bvh calculation
  scene_set_dirty(s, RT_MESH);
}

uint32_t scene_add_quad(scene *s, uint32_t subx, uint32_t suby, uint32_t mtl)
{
  // Generate plane in XZ at origin with normal pointing up in Y
  mesh *m = &s->meshes[s->mesh_cnt];
  mesh_init(m, 2 * subx * suby);

  float P = PLANE_DEFAULT_SIZE;
  float P2 = 0.5f * P;
  float dx = P / (float)subx;
  float dz = P / (float)suby;

  float z = 0.0f;
  for(uint32_t j=0; j<suby; j++) {
    float x = 0.0f;
    for(uint32_t i=0; i<subx; i++) {
      uint32_t ofs = 2 * (subx * j + i);
      tri *tri = &m->tris[ofs];
      tri->v0 = (vec3){ -P2 + x, 0.0, -P2 + z };
      tri->v1 = (vec3){ -P2 + x, 0.0, -P2 + z + dz };
      tri->v2 = (vec3){ -P2 + x + dx, 0.0, -P2 + z + dz };
      tri->n0 = tri->n1 = tri->n2 = (vec3){ 0.0f, 1.0f, 0.0f };
#ifdef TEXTURE_SUPPORT // Untested
      tri->uv0[0] = x / P; tri->uv0[1] = z / P;
      tri->uv1[0] = x / P; tri->uv1[1] = (z + dz) / P;
      tri->uv2[0] = (x + dx) / P; tri->uv2[1] = (z + dz) / P;
#endif
      tri->mtl = mtl;
      m->centers[ofs] = tri_calc_center(tri);

      tri = &m->tris[ofs + 1];
      tri->v0 = (vec3){ -P2 + x, 0.0, -P2 + z };
      tri->v1 = (vec3){ -P2 + x + dx, 0.0, -P2 + z + dz };
      tri->v2 = (vec3){ -P2 + x + dx, 0.0, -P2 + z };
      tri->n0 = tri->n1 = tri->n2 = (vec3){ 0.0f, 1.0f, 0.0f };
#ifdef TEXTURE_SUPPORT // Untested
      tri->uv0[0] = x / P; tri->uv0[1] = z / P;
      tri->uv1[0] = (x + dx) / P; tri->uv1[1] = (z + dz) / P;
      tri->uv2[0] = (x + dx) / P; tri->uv2[1] = z / P;
#endif
      tri->mtl = mtl;
      m->centers[ofs + 1] = tri_calc_center(tri);

      x += dx;
    }
    z += dz;
  }

  finalize_mesh(s, m, mtl);
  return s->mesh_cnt++;
}

uint32_t scene_add_icosphere(scene *s, uint8_t steps, uint32_t mtl, bool faceNormals)
{
  mesh *m = &s->meshes[s->mesh_cnt];
  mesh_init(m, 20 * (1 << (2 * steps)));

  float phi = 0.5f * (1.0f + sqrtf(5.0f));
  
  m->tris[ 0] = (tri){ .v0 = (vec3){ -1.0, phi, 0.0 }, .v1 = (vec3){ 0.0, 1.0, phi }, .v2 = (vec3){ 1.0, phi, 0.0 } };
  m->tris[ 1] = (tri){ .v0 = (vec3){ 1.0, phi, 0.0 }, .v1 = (vec3){ 0.0, 1.0, -phi }, .v2 = (vec3){ -1.0, phi, 0.0 } };
  m->tris[ 2] = (tri){ .v0 = (vec3){ 1.0, phi, 0.0 }, .v1 = (vec3){ 0.0, 1.0, phi }, .v2 = (vec3){ phi, 0.0, 1.0 } };
  m->tris[ 3] = (tri){ .v0 = (vec3){ 1.0, phi, 0.0 }, .v1 = (vec3){ phi, 0.0, -1.0 }, .v2 = (vec3){ 0.0, 1.0, -phi } };
  m->tris[ 4] = (tri){ .v0 = (vec3){ phi, 0.0, -1.0 }, .v1 = (vec3){ 1.0, phi, 0.0 }, .v2 = (vec3){ phi, 0.0, 1.0 } };
  m->tris[ 5] = (tri){ .v0 = (vec3){ -1.0, -phi, 0.0 }, .v1 = (vec3){ 1.0, -phi, 0.0 }, .v2 = (vec3){ 0.0, -1.0, phi } };
  m->tris[ 6] = (tri){ .v0 = (vec3){ -1.0, -phi, 0.0 }, .v1 = (vec3){ 0.0, -1.0, -phi }, .v2 = (vec3){ 1.0, -phi, 0.0 } };
  m->tris[ 7] = (tri){ .v0 = (vec3){ -1.0, -phi, 0.0 }, .v1 = (vec3){ 0.0, -1.0, phi }, .v2 = (vec3){ -phi, 0.0, 1.0 } };
  m->tris[ 8] = (tri){ .v0 = (vec3){ -1.0, -phi, 0.0 }, .v1 = (vec3){ -phi, 0.0, -1.0 }, .v2 = (vec3){ 0.0, -1.0, -phi } };
  m->tris[ 9] = (tri){ .v0 = (vec3){ -phi, 0.0, 1.0 }, .v1 = (vec3){ -phi, 0.0, -1.0 }, .v2 = (vec3){ -1.0, -phi, 0.0 } };
  m->tris[10] = (tri){ .v0 = (vec3){ -1.0, phi, 0.0 }, .v1 = (vec3){ -phi, 0.0, 1.0 }, .v2 = (vec3){ 0.0, 1.0, phi } };
  m->tris[11] = (tri){ .v0 = (vec3){ -1.0, phi, 0.0 }, .v1 = (vec3){ 0.0, 1.0, -phi }, .v2 = (vec3){ -phi, 0.0, -1.0 } };
  m->tris[12] = (tri){ .v0 = (vec3){ -1.0, phi, 0.0 }, .v1 = (vec3){ -phi, 0.0, -1.0 }, .v2 = (vec3){ -phi, 0.0, 1.0 } };
  m->tris[13] = (tri){ .v0 = (vec3){ 1.0, -phi, 0.0 }, .v1 = (vec3){ phi, 0.0, 1.0 }, .v2 = (vec3){ 0.0, -1.0, phi } };
  m->tris[14] = (tri){ .v0 = (vec3){ 1.0, -phi, 0.0 }, .v1 = (vec3){ 0.0, -1.0, -phi }, .v2 = (vec3){ phi, 0.0, -1.0 } };
  m->tris[15] = (tri){ .v0 = (vec3){ 1.0, -phi, 0.0 }, .v1 = (vec3){ phi, 0.0, -1.0 }, .v2 = (vec3){ phi, 0.0, 1.0 } };
  m->tris[16] = (tri){ .v0 = (vec3){ 0.0, -1.0, -phi }, .v1 = (vec3){ -phi, 0.0, -1.0 }, .v2 = (vec3){ 0.0, 1.0, -phi } };
  m->tris[17] = (tri){ .v0 = (vec3){ 0.0, -1.0, -phi }, .v1 = (vec3){ 0.0, 1.0, -phi }, .v2 = (vec3){ phi, 0.0, -1.0 } };
  m->tris[18] = (tri){ .v0 = (vec3){ 0.0, 1.0, phi }, .v1 = (vec3){ -phi, 0.0, 1.0 }, .v2 = (vec3){ 0.0, -1.0, phi } };
  m->tris[19] = (tri){ .v0 = (vec3){ 0.0, 1.0, phi }, .v1 = (vec3){ 0.0, -1.0, phi }, .v2 = (vec3){ phi, 0.0, 1.0 } };

  for(uint8_t i=0; i<20; i++) {
    tri *t = &m->tris[i];
    t->v0 = vec3_unit(t->v0);
    t->v1 = vec3_unit(t->v1);
    t->v2 = vec3_unit(t->v2);
  }

  // Alloc temp tri buffer for last but one subdiv level
  tri *temp_tris = (steps > 0) ? malloc(20 * (1 << (2 * (steps - 1))) * sizeof(*temp_tris)) : NULL;

  // Subdivide
  for(uint8_t j=0; j<steps; j++)
  {
    uint32_t curr_tri_cnt = 20 * (1 << (2 * j)); 
    
    memcpy(temp_tris, m->tris, curr_tri_cnt * sizeof(*temp_tris));

    for(uint32_t i=0; i<curr_tri_cnt; i++) {
      tri *t = &temp_tris[i];
      vec3 a = vec3_unit(vec3_scale(vec3_add(t->v0, t->v1), 0.5f));
      vec3 b = vec3_unit(vec3_scale(vec3_add(t->v1, t->v2), 0.5f));
      vec3 c = vec3_unit(vec3_scale(vec3_add(t->v2, t->v0), 0.5f));

      uint32_t ofs = 4 * i;
      m->tris[ofs + 0] = (tri){ .v0 = t->v0, .v1 =     a, .v2 =     c };
      m->tris[ofs + 1] = (tri){ .v0 =     a, .v1 = t->v1, .v2 =     b };
      m->tris[ofs + 2] = (tri){ .v0 =     c, .v1 =     b, .v2 = t->v2 };
      m->tris[ofs + 3] = (tri){ .v0 =     a, .v1 =     b, .v2 =     c };
    }
  }

  free(temp_tris);

  // Materials, centers, normals and uvs
  for(uint32_t i=0; i<m->tri_cnt; i++) {
    tri *t = &m->tris[i];
    t->mtl = mtl;
    m->centers[i] = tri_calc_center(t);
    if(faceNormals) {
      vec3 n = vec3_unit(m->centers[i]);
      t->n0 = n;
      t->n1 = n;
      t->n2 = n;
    } else {
      t->n0 = t->v0;
      t->n1 = t->v1;
      t->n2 = t->v2;
    }
#ifdef TEXTURE_SUPPORT // Untested
    t->uv0[0] = asinf(t->v0.x) / PI + 0.5f;
    t->uv0[1] = asinf(t->v0.y) / PI + 0.5f;
    t->uv1[0] = asinf(t->v1.x) / PI + 0.5f;
    t->uv1[1] = asinf(t->v1.y) / PI + 0.5f;
    t->uv2[0] = asinf(t->v2.x) / PI + 0.5f;
    t->uv2[1] = asinf(t->v2.y) / PI + 0.5f;
#endif
  }

  finalize_mesh(s, m, mtl);
  return s->mesh_cnt++;
}

uint32_t scene_add_uvsphere(scene *s, float radius, uint32_t subx, uint32_t suby, uint32_t mtl, bool faceNormals)
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
      
      t1->mtl = mtl;
      m->centers[ofs] = tri_calc_center(t1);
      
      if(faceNormals) {
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
      
      t2->mtl = mtl;
      m->centers[ofs + 1] = tri_calc_center(t2);
      
      if(faceNormals) {
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

  finalize_mesh(s, m, mtl);
  return s->mesh_cnt++;
}

uint32_t scene_add_uvcylinder(scene *s, float radius, float height, uint32_t subx, uint32_t suby, uint32_t mtl, bool faceNormals)
{
  mesh *m = &s->meshes[s->mesh_cnt];
  mesh_init(m, 2 * subx * suby);

  float dphi = 2 * PI / subx;
  float dh = height / suby;
  float inv_r = 1.0f / radius;

  float h = 0.0f;
  for(uint32_t j=0; j<suby; j++) {
    float phi = 0.0f;
    for(uint32_t i=0; i<subx; i++) {
      uint32_t ofs = 2 * (subx * j + i);

      float x1 = radius * sinf(phi);
      float x2 = radius * sinf(phi + dphi);
      float y1 = -height * 0.5f + h;
      float y2 = -height * 0.5f + h + dh;
      float z1 = radius * cosf(phi);
      float z2 = radius * cosf(phi + dphi);

      vec3 a = { x1, y2, z1 };
      vec3 b = { x1, y1, z1 };
      vec3 c = { x2, y1, z2 };
      vec3 d = { x2, y2, z2 };

      tri *t1 = &m->tris[ofs];
      *t1 = (tri){ .v0 = a, .v1 = b, .v2 = c,
#ifdef TEXTURE_SUPPORT // Untested
        .uv0[0] = phi / 2.0f * PI, .uv0[1] = (h + dh) / height,
        .uv1[0] = phi / 2.0f * PI, .uv1[1] = h / height,
        .uv2[0] = (phi + dphi) / 2.0f * PI, .uv2[1] = h / height,
#endif
      };
      
      t1->mtl = mtl;
      m->centers[ofs] = tri_calc_center(t1);
      
      if(faceNormals) {
        t1->n0 = vec3_unit(vec3_cross(vec3_sub(a, b), vec3_sub(c, b)));
        t1->n1 = t1->n0;
        t1->n2 = t1->n0;
      } else {
        t1->n0 = vec3_scale((vec3){ x1, 0.0f, z1 }, inv_r);
        t1->n1 = t1->n0;
        t1->n2 = vec3_scale((vec3){ x2, 0.0f, z2 }, inv_r);
      }

      tri *t2 = &m->tris[ofs + 1];
      *t2 = (tri){ .v0 = a, .v1 = c, .v2 = d,
#ifdef TEXTURE_SUPPORT // Untested
        .uv0[0] = t1->uv0[0], .uv0[1] = t1->uv0[1],
        .uv1[0] = t1->uv2[0], .uv1[1] = t1->uv1[1],
        .uv2[0] = t1->uv2[0], .uv2[1] = t1->uv0[1],
#endif
      };
      
      t2->mtl = mtl;
      m->centers[ofs + 1] = tri_calc_center(t2);
      
      if(faceNormals) {
        t2->n0 = vec3_unit(vec3_cross(vec3_sub(a, c), vec3_sub(d, c)));
        t2->n1 = t2->n0;
        t2->n2 = t2->n0;
      } else {
        t2->n0 = vec3_scale((vec3){ x1, 0.0f, z1 }, inv_r);
        t2->n1 = vec3_scale((vec3){ x2, 0.0f, z2 }, inv_r);
        t2->n2 = t2->n1;
      }

      phi += dphi;
    }

    h += dh;
  }

  finalize_mesh(s, m, mtl);
  return s->mesh_cnt++;
}

uint32_t scene_add_mesh(scene *s, const uint8_t *data, uint32_t mtl, bool faceNormals)
{
  uint32_t ofs = 0;

  uint32_t vertex_cnt = *(uint32_t *)(data + ofs);
  ofs += sizeof(vertex_cnt);

  uint32_t normal_cnt = *(uint32_t *)(data + ofs);
  ofs += sizeof(normal_cnt);

  uint32_t uv_cnt = *(uint32_t *)(data + ofs);
  ofs += sizeof(uv_cnt);

  uint32_t tri_cnt = *(uint32_t *)(data + ofs);
  ofs += sizeof(tri_cnt);

  float *vertices = (float *)(data + ofs);
  ofs += vertex_cnt * 3 * sizeof(*vertices);

  float *normals = (float *)(data + ofs);
  ofs += normal_cnt * 3 * sizeof(*normals);

  float *uvs = (float *)(data + ofs);
  ofs += uv_cnt * 2 * sizeof(*uvs);

  uint32_t *indices = (uint32_t *)(data + ofs);

  mesh *m = &s->meshes[s->mesh_cnt];
  mesh_init(m, tri_cnt);

  uint32_t items = (1 + (uv_cnt > 0 ? 1 : 0) + (normal_cnt > 0 ? 1 : 0));

  for(uint32_t i=0; i<tri_cnt; i++) {
    tri *t = &m->tris[i];
    uint32_t index = 3 * items * i;

    memcpy(&t->v0, &vertices[3 * indices[index]], sizeof(t->v0));
    memcpy(&t->v1, &vertices[3 * indices[index + items]], sizeof(t->v1));
    memcpy(&t->v2, &vertices[3 * indices[index + items + items]], sizeof(t->v2));
#ifdef TEXTURE_SUPPORT // Untested
    if(uv_cnt > 0 ) {
      memcpy(t->uv0, &uvs[2 * indices[index + 1]], 2 * sizeof(*t->uv0));
      memcpy(t->uv1, &uvs[2 * indices[index + items + 1]], 2 * sizeof(*t->uv1));
      memcpy(t->uv2, &uvs[2 * indices[index + items + items + 1]], 2 * sizeof(*t->uv2));
    }
#endif
    if(!faceNormals && normal_cnt > 0) {
      memcpy(&t->n0, &normals[3 * indices[index + 2]], sizeof(t->n0));
      memcpy(&t->n1, &normals[3 * indices[index + items + 2]], sizeof(t->n1));
      memcpy(&t->n2, &normals[3 * indices[index + items + items + 2]], sizeof(t->n2));
    } else 
      t->n0 = t->n1 = t->n2 = vec3_unit(vec3_cross(vec3_sub(t->v0, t->v1), vec3_sub(t->v2, t->v1)));

    t->mtl = mtl;
    m->centers[i] = tri_calc_center(t);
  }

  finalize_mesh(s, m, mtl);
  return s->mesh_cnt++;
}
