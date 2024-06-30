#include "scene.h"
#include "../acc/bvh.h"
#include "../acc/tlas.h"
#include "../settings.h"
#include "../sys/mutil.h"
#include "../sys/sutil.h"
#include "../sys/log.h"
#include "../util/aabb.h"
#include "cam.h"
#include "inst.h"
#include "mesh.h"
#include "mtl.h"
#include "tri.h"

void scene_init(scene *s, uint32_t mesh_cnt, uint32_t mtl_cnt, uint32_t cam_cnt, uint32_t inst_cnt)
{
  s->mtls         = malloc(mtl_cnt * sizeof(*s->mtls)); 
  s->max_mtl_cnt  = mtl_cnt;
  s->mtl_cnt      = 0;
 
  s->meshes       = malloc(mesh_cnt * sizeof(*s->meshes));
  s->max_mesh_cnt = mesh_cnt;
  s->mesh_cnt     = 0;
 
  s->bvhs         = malloc(mesh_cnt * sizeof(*s->bvhs));
  s->bvh_cnt      = 0;
  
  s->instances    = malloc(inst_cnt * sizeof(*s->instances));
  s->inst_info    = malloc(inst_cnt * sizeof(*s->inst_info));
  s->tlas_nodes   = malloc(2 * inst_cnt * sizeof(*s->tlas_nodes));
  s->max_inst_cnt = inst_cnt;
  s->inst_cnt     = 0;
  
  s->ltris        = NULL; // Attach all meshes before initializing ltris
  s->max_ltri_cnt = 0;
  s->ltri_cnt     = 0;

  s->cams         = malloc(cam_cnt * sizeof(*s->cams));
  s->cam_cnt      = cam_cnt;
  s->active_cam   = &s->cams[0];

  s->max_tri_cnt  = 0; // Attach all meshes before calculating this

  s->bg_col       = (vec3){ 0.0f, 0.0f, 0.0f };

  s->curr_ofs     = 0;

  scene_set_dirty(s, RT_CAM_VIEW);
}

void scene_release(scene *s)
{
  for(uint32_t i=0; i<s->mesh_cnt; i++)
    mesh_release(&s->meshes[i]);

  free(s->cams);
  free(s->ltris);
  free(s->tlas_nodes);
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

void scene_prepare_ltris(scene *s)
{
  if(!s->ltris) {
    // Count the tris and ltris of all the meshes
    uint32_t ltri_cnt = 0;
    uint32_t tri_cnt = 0;
    for(uint32_t i=0; i<s->mesh_cnt; i++) {
      mesh *m = &s->meshes[i];
      if(m->is_emissive)
        ltri_cnt += m->tri_cnt;
      tri_cnt += m->tri_cnt;
    }

    // Remeber total tri cnt (renderer needs to reserve gpu resources)
    s->max_tri_cnt = tri_cnt;

    // Allocate space for given ltri cnt
    s->max_ltri_cnt = ltri_cnt;
    s->ltris = malloc(ltri_cnt * sizeof(*s->ltris));
  }
}

void scene_finalize(scene *s)
{
  scene_build_bvhs(s);
  scene_prepare_ltris(s);
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
          inst->id & 0xffff, i, info->transform,
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
      if(mtl->emissive > 0.0f) {
        uint32_t ltri_id = ltri_ofs + ltri_cnt++;
        tri_build_ltri(&s->ltris[ltri_id], t,
            inst->id & 0xffff, i, info->transform, mtl->col);
        t->ltri_id = ltri_id;
      }
    }
  }

  // Remember for future updates
  info->ltri_ofs = ltri_ofs;
  info->ltri_cnt = ltri_cnt;

  scene_set_dirty(s, RT_LTRI);
}

void update_ltris(scene *s, inst_info *info)
{
  tri *tris = s->meshes[info->mesh_shape].tris;
  for(uint32_t i=0; i<info->ltri_cnt; i++) {
    ltri *lt = &s->ltris[info->ltri_ofs + i];
    tri_update_ltri(lt, &tris[lt->tri_id], info->transform);
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
          update_ltris(s, info);

        // Calc inverse transform
        mat4 inv_transform;
        mat4_inv(inv_transform, info->transform);
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
            float P = 0.5f * QUAD_DEFAULT_SIZE;
            mi = (vec3){ -P, -EPSILON, -P };
            ma = (vec3){  P,  EPSILON,  P };
          }
        } else {
          // Mesh type, use (object space) BVH root nodes
          mi = s->bvhs[info->mesh_shape].nodes[0].min;
          ma = s->bvhs[info->mesh_shape].nodes[0].max;
        }

        // Transform instance aabb to world space
        aabb_grow(&a, mat4_mul_pos(info->transform, (vec3){ mi.x, mi.y, mi.z }));
        aabb_grow(&a, mat4_mul_pos(info->transform, (vec3){ ma.x, mi.y, mi.z }));
        aabb_grow(&a, mat4_mul_pos(info->transform, (vec3){ mi.x, ma.y, mi.z }));
        aabb_grow(&a, mat4_mul_pos(info->transform, (vec3){ ma.x, ma.y, mi.z }));
        aabb_grow(&a, mat4_mul_pos(info->transform, (vec3){ mi.x, mi.y, ma.z }));
        aabb_grow(&a, mat4_mul_pos(info->transform, (vec3){ ma.x, mi.y, ma.z }));
        aabb_grow(&a, mat4_mul_pos(info->transform, (vec3){ mi.x, ma.y, ma.z }));
        aabb_grow(&a, mat4_mul_pos(info->transform, (vec3){ ma.x, ma.y, ma.z }));

        inst->min = a.min;
        inst->max = a.max;
      }

      info->state &= ~IS_TRANS_DIRTY;
      rebuild_tlas = true;
    }

    info->state &= ~(IS_MTL_DIRTY | IS_WAS_EMISSIVE);
    ltri_cnt += info->ltri_cnt;
  }

  // Current ltri cnt in the scene
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
}

// Material updates can trigger light tri changes, so not exposing mtl update for now.
void scene_upd_mtl(scene *s, uint32_t mtl_id, mtl *mtl)
{
  if(&s->mtls[mtl_id] != mtl)
    memcpy(&s->mtls[mtl_id], mtl, sizeof(*s->mtls));

  scene_set_dirty(s, RT_MTL);
}

uint32_t scene_add_mtl(scene *s, mtl *mtl)
{
  scene_upd_mtl(s, s->mtl_cnt, mtl);
  return s->mtl_cnt++;
}

void scene_set_active_cam(scene *s, uint32_t cam_id)
{
  s->active_cam = &s->cams[cam_id];
}

cam *scene_get_active_cam(scene *s)
{
  return s->active_cam;
}

cam *scene_get_cam(scene *s, uint32_t cam_id)
{
  return &s->cams[cam_id];
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

uint32_t scene_add_mesh_inst(scene *s, uint32_t mesh_id, int32_t mtl_id, mat4 transform)
{
  // Bit 30 not set indicates mesh type
  return add_inst(s, mesh_id & MESH_SHAPE_MASK, mtl_id, transform);
}

uint32_t scene_add_shape_inst(scene *s, shape_type shape, uint16_t mtl_id, mat4 transform)
{
  // Bit 30 set indicates shape type
  // Shape types are always using the material override
  return add_inst(s, SHAPE_TYPE_BIT | (shape & MESH_SHAPE_MASK), mtl_id, transform);
}

void scene_upd_inst_trans(scene *s, uint32_t inst_id, mat4 transform)
{
  inst_info *info = &s->inst_info[inst_id];
  memcpy(info->transform, transform, sizeof(mat4));
  info->state |= IS_TRANS_DIRTY;
}

void scene_upd_inst_mtl(scene *s, uint32_t inst_id, int32_t mtl_id)
{
  inst *inst = &s->instances[inst_id];
  inst_info *info = &s->inst_info[inst_id];

  // Flag instances that were emissive before (in case this changes)
  if(info->state & IS_EMISSIVE)
    info->state |= IS_WAS_EMISSIVE;

  // Mtl override if mtl_id >= 0
  if(mtl_id >= 0) {
    // Highest 16 bits are mtl override id, i.e. max 65536 materials
    inst->id = (mtl_id << 16) | (inst->id & INST_ID_MASK);
    
    // Set highest bit to enable the material override
    inst->data |= MTL_OVERRIDE_BIT;

    // For mesh inst only we want to know if mtl is now emissive or not
    if(!(inst->data & SHAPE_TYPE_BIT)) {
      if(s->mtls[mtl_id].emissive > 0.0f)
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

mesh *scene_acquire_mesh(scene *s)
{
  return &s->meshes[s->mesh_cnt];
}

uint32_t scene_attach_mesh(scene *s, mesh *m, bool is_mesh_emissive)
{
  // Set and update offset into tri buffer
  m->ofs = s->curr_ofs;
  s->curr_ofs += m->tri_cnt;

  // Meshes with emissive mtl receive special treatment
  m->is_emissive = is_mesh_emissive;

  // Flag to trigger bvh calculation
  scene_set_dirty(s, RT_MESH);

  // Return mesh id
  return s->mesh_cnt++;
}
