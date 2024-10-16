#include "scene.h"

#include "../base/math.h"
#include "../base/string.h"
#include "../base/walloc.h"
#include "../rend/bvh.h"
#include "../util/aabb.h"
#include "cam.h"
#include "inst.h"
#include "mesh.h"
#include "mtl.h"
#include "tri.h"

void scene_init(scene *s, uint16_t max_mesh_cnt, uint16_t max_mtl_cnt,
                uint16_t max_cam_cnt, uint16_t max_inst_cnt)
{
  s->mtls = malloc(max_mtl_cnt * sizeof(*s->mtls));
  s->max_mtl_cnt = max_mtl_cnt;
  s->mtl_cnt = 0;

  s->meshes = malloc(max_mesh_cnt * sizeof(*s->meshes));
  s->max_mesh_cnt = max_mesh_cnt;
  s->mesh_cnt = 0;

  s->instances = malloc(max_inst_cnt * sizeof(*s->instances));
  s->inst_info = malloc(max_inst_cnt * sizeof(*s->inst_info));
  s->tlas_nodes = malloc(2 * max_inst_cnt * sizeof(*s->tlas_nodes));
  s->max_inst_cnt = max_inst_cnt;
  s->inst_cnt = 0;

  s->ltris = NULL; // Attach all meshes before initializing ltris
  s->tri_ids = NULL;
  s->max_ltri_cnt = 0;
  s->ltri_cnt = 0;

  s->max_tri_cnt = 0; // Attach all meshes before calculating this
  s->blas_nodes = NULL;

  s->cams = malloc(max_cam_cnt * sizeof(*s->cams));
  s->cam_cnt = max_cam_cnt;
  s->active_cam = &s->cams[0];

  s->bg_col = (vec3){0.0f, 0.0f, 0.0f};

  s->curr_ofs = 0;

  scene_set_dirty(s, RT_CFG);
  scene_set_dirty(s, RT_CAM);
}

void scene_release(scene *s)
{
  for(uint16_t i = 0; i < s->mesh_cnt; i++)
    mesh_release(&s->meshes[i]);

  free(s->cams);
  free(s->blas_nodes);
  free(s->tri_ids);
  free(s->ltris);
  free(s->tlas_nodes);
  free(s->inst_info);
  free(s->instances);
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

void scene_finalize(scene *s)
{
  // Count tris and ltris of all the meshes
  s->max_tri_cnt = 0;
  s->max_ltri_cnt = 0;
  for(uint16_t i = 0; i < s->mesh_cnt; i++) {
    mesh *m = &s->meshes[i];
    if(m->is_emissive)
      s->max_ltri_cnt += m->tri_cnt;
    s->max_tri_cnt += m->tri_cnt;
  }

  // Allocate space for ltris and tri_ids (point from ltris to tris)
  s->ltris = malloc(s->max_ltri_cnt * sizeof(*s->ltris));
  s->tri_ids = malloc(s->max_ltri_cnt * sizeof(*s->tri_ids));

  // Allocate enough blas nodes to cover the tris of all meshes
  s->blas_nodes = malloc(2 * s->max_tri_cnt * sizeof(*s->blas_nodes));

  // Build a blas for each mesh
  for(uint16_t i = 0; i < s->mesh_cnt; i++) {
    mesh *m = &s->meshes[i];
    blas_build(&s->blas_nodes[2 * m->ofs], m->tris, m->tri_cnt);
  }

  scene_set_dirty(s, RT_BLAS);
}

void build_ltris(scene *s, inst *inst, inst_info *info, uint32_t ltri_ofs)
{
  mesh *m = &s->meshes[info->mesh_id];
  uint32_t tri_cnt = m->tri_cnt;
  tri *t = m->tris;
  tri_nrm *tn = m->tri_nrms;
  uint32_t ltri_cnt = 0;

  if(inst->data & MTL_OVERRIDE_BIT) {
    // Material override applies to all tris, create a ltri for each tri
    for(uint32_t i = 0; i < tri_cnt; i++) {
      uint32_t ltri_id = ltri_ofs + ltri_cnt++;
      tri_build_ltri(&s->ltris[ltri_id], t, tn, info->transform,
                     info->inv_transform, s->mtls[tn->mtl & 0xffff].col);
      // In case of CPU-side updates we need to know which tri a ltri references
      s->tri_ids[ltri_id] = i;
      // Vice versa, a tri also links its ltri. Tris that emit light need to be
      // unique (multiple instances of the same light mesh/tri are not allowed)
      tn->ltri_id = ltri_id;
      t++;
      tn++;
    }
  } else {
    // Create ltris for emissive tris of the mesh only
    for(uint32_t i = 0; i < tri_cnt; i++) {
      mtl *mtl = &s->mtls[tn->mtl & 0xffff];
      if(mtl->emissive > 0.0f) {
        uint32_t ltri_id = ltri_ofs + ltri_cnt++;
        tri_build_ltri(&s->ltris[ltri_id], t, tn, info->transform,
                       info->inv_transform, mtl->col);
        s->tri_ids[ltri_id] = i;
        tn->ltri_id = ltri_id;
      }
      t++;
      tn++;
    }
  }

  // Remember for future updates
  info->ltri_ofs = ltri_ofs;
  info->ltri_cnt = ltri_cnt;

  scene_set_dirty(s, RT_TRI); // Tris have modified ltri ids
  scene_set_dirty(s, RT_LTRI);
}

void update_ltris(scene *s, inst_info *info)
{
  tri *tris = s->meshes[info->mesh_id].tris;
  tri_nrm *tri_nrms = s->meshes[info->mesh_id].tri_nrms;

  for(uint32_t i = 0; i < info->ltri_cnt; i++) {
    uint32_t ltri_id = info->ltri_ofs + i;
    uint32_t tri_id = s->tri_ids[ltri_id];
    ltri *lt = &s->ltris[ltri_id];
    tri_update_ltri(lt, &tris[tri_id], &tri_nrms[tri_id], info->transform,
                    info->inv_transform);
  }

  scene_set_dirty(s, RT_LTRI);
}

void scene_prepare_render(scene *s)
{
  // Update instances
  bool rebuild_tlas = false;
  bool rebuild_ltris = false;
  uint32_t ltri_cnt = 0;
  for(uint32_t i = 0; i < s->inst_cnt; i++) {
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
          update_ltris(s, info);

        // Instances receives inverse transform only
        memcpy(inst->inv_transform, info->inv_transform, 12 * sizeof(float));

        // Root node object-space aabb
        vec3 mi, ma;
        node *n = &s->blas_nodes[2 * (inst->data & INST_DATA_MASK)];
        mi = vec3_min(n->lmin, n->rmin);
        ma = vec3_max(n->lmax, n->rmax);

        // Transform instance aabb to world space
        aabb *a = &info->box;
        *a = aabb_init();
        aabb_grow(a, mat4_mul_pos(info->transform, (vec3){mi.x, mi.y, mi.z}));
        aabb_grow(a, mat4_mul_pos(info->transform, (vec3){ma.x, mi.y, mi.z}));
        aabb_grow(a, mat4_mul_pos(info->transform, (vec3){mi.x, ma.y, mi.z}));
        aabb_grow(a, mat4_mul_pos(info->transform, (vec3){ma.x, ma.y, mi.z}));
        aabb_grow(a, mat4_mul_pos(info->transform, (vec3){mi.x, mi.y, ma.z}));
        aabb_grow(a, mat4_mul_pos(info->transform, (vec3){ma.x, mi.y, ma.z}));
        aabb_grow(a, mat4_mul_pos(info->transform, (vec3){mi.x, ma.y, ma.z}));
        aabb_grow(a, mat4_mul_pos(info->transform, (vec3){ma.x, ma.y, ma.z}));
      }

      info->state &= ~IS_TRANS_DIRTY;
      rebuild_tlas = true;
    }

    info->state &= ~(IS_MTL_DIRTY | IS_WAS_EMISSIVE);
    ltri_cnt += info->ltri_cnt;
  }

  // Current ltri cnt in the scene
  s->ltri_cnt = ltri_cnt;

  if(rebuild_tlas) {
    tlas_build(s->tlas_nodes, s->inst_info, s->inst_cnt);
    scene_set_dirty(s, RT_INST);
  }
}

// Material updates can trigger light tri changes, so not exposing mtl update
// for now.
void scene_upd_mtl(scene *s, uint16_t mtl_id, mtl *mtl)
{
  if(&s->mtls[mtl_id] != mtl)
    memcpy(&s->mtls[mtl_id], mtl, sizeof(*s->mtls));

  scene_set_dirty(s, RT_MTL);
}

uint16_t scene_add_mtl(scene *s, mtl *mtl)
{
  scene_upd_mtl(s, s->mtl_cnt, mtl);
  return s->mtl_cnt++;
}

void scene_set_active_cam(scene *s, uint16_t cam_id)
{
  s->active_cam = &s->cams[cam_id];
}

cam *scene_get_active_cam(scene *s)
{
  return s->active_cam;
}

cam *scene_get_cam(scene *s, uint16_t cam_id)
{
  return &s->cams[cam_id];
}

uint16_t scene_add_inst(scene *s, uint16_t mesh_id, uint16_t mtl_id,
                        uint32_t flags, mat4 transform)
{
  inst_info *info = &s->inst_info[s->inst_cnt];
  info->mesh_id = mesh_id;
  info->state = IS_TRANS_DIRTY | IS_MTL_DIRTY;
  info->ltri_ofs = 0;
  info->ltri_cnt = 0;

  inst *inst = &s->instances[s->inst_cnt];

  // Lowest 16 bits are instance id, i.e. max 65536 instances
  inst->id = s->inst_cnt & INST_ID_MASK;

  // Tri ofs and blas node ofs * 2
  inst->data = s->meshes[mesh_id].ofs;

  // Flags (gpu payload)
  inst->flags = flags;

  // Set transform and mtl override id to instance
  scene_upd_inst_trans(s, s->inst_cnt, transform);
  scene_upd_inst_mtl(s, s->inst_cnt, mtl_id);

  return s->inst_cnt++;
}

void scene_upd_inst_trans(scene *s, uint16_t inst_id, mat4 transform)
{
  inst_info *info = &s->inst_info[inst_id];
  memcpy(info->transform, transform, sizeof(mat4));
  mat4_inv(info->inv_transform, transform);
  info->state |= IS_TRANS_DIRTY;
}

void scene_upd_inst_mtl(scene *s, uint16_t inst_id, uint16_t mtl_id)
{
  inst *inst = &s->instances[inst_id];
  inst_info *info = &s->inst_info[inst_id];

  // Flag instances that were emissive before (in case this changes)
  if(info->state & IS_EMISSIVE)
    info->state |= IS_WAS_EMISSIVE;

  // Mtl override if mtl_id != 0xffff
  if(mtl_id < NO_MTL_OVERRIDE) {
    // Highest 16 bits are mtl override id, i.e. max 65535 - 1 materials
    inst->id = (mtl_id << 16) | (inst->id & INST_ID_MASK);

    // Set highest bit to enable the material override
    inst->data |= MTL_OVERRIDE_BIT;

    // We want to know if mtl is now emissive or not
    if(s->mtls[mtl_id].emissive > 0.0f)
      info->state |= IS_EMISSIVE;
    else
      info->state &= ~IS_EMISSIVE;
  }
  // Reset mtl override
  else {
    inst->data = inst->data & INST_DATA_MASK;

    // Flag instance if mesh is emissive
    if(s->meshes[info->mesh_id].is_emissive)
      info->state |= IS_EMISSIVE;
    else
      info->state &= ~IS_EMISSIVE;
  }

  s->inst_info[inst_id].state |= IS_MTL_DIRTY;
}

void scene_upd_inst_flags(scene *s, uint16_t inst_id, uint32_t flags)
{
  s->instances[inst_id].flags = flags;
  scene_set_dirty(s, RT_INST);
}

void scene_set_inst_state(scene *s, uint16_t inst_id, uint32_t state)
{
  s->inst_info[inst_id].state |= state;

  // If something gets disabled, tlas and ltri rebuild might be necessary
  if(state & IS_DISABLED)
    s->inst_info[inst_id].state |= IS_TRANS_DIRTY | IS_MTL_DIRTY;
}

void scene_clr_inst_state(scene *s, uint16_t inst_id, uint32_t state)
{
  s->inst_info[inst_id].state &= ~state;

  // If something gets enabled, tlas and ltri rebuild might be necessary
  if(state & IS_DISABLED)
    s->inst_info[inst_id].state |= IS_TRANS_DIRTY | IS_MTL_DIRTY;
}

uint32_t scene_get_inst_state(scene *s, uint16_t inst_id)
{
  return s->inst_info[inst_id].state;
}

mesh *scene_acquire_mesh(scene *s)
{
  return &s->meshes[s->mesh_cnt];
}

uint16_t scene_attach_mesh(scene *s, mesh *m, bool is_mesh_emissive)
{
  scene_set_dirty(s, RT_TRI);

  // Set and update offset into tri buffer
  m->ofs = s->curr_ofs;
  s->curr_ofs += m->tri_cnt;

  // Meshes with emissive mtl receive special treatment
  m->is_emissive = is_mesh_emissive;

  // Return mesh id
  return s->mesh_cnt++;
}
