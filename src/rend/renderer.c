#include "renderer.h"
#include "../acc/bvh.h"
#include "../acc/lighttree.h"
#include "../acc/tlas.h"
#include "../scene/cam.h"
#include "../scene/inst.h"
#include "../scene/mesh.h"
#include "../scene/mtl.h"
#include "../scene/scene.h"
#include "../scene/tri.h"
#include "../settings.h"
#include "../sys/log.h"
#include "../sys/mutil.h"
#include "../sys/sutil.h"

#ifdef NATIVE_BUILD
#include <SDL.h>
#include "../util/ray.h"
#include "intersect.h"
#endif

#define TEMPORAL_WEIGHT       0.0f

// Global uniform buffer offsets
#define GLOB_BUF_OFS_CFG      0
#define GLOB_BUF_OFS_FRAME    16
#define GLOB_BUF_OFS_CAM      48
#define GLOB_BUF_OFS_VIEW     96
#define GLOB_BUF_SIZE         144

#define MAX_BOUNCES           5

#define MAX_UNIFORM_BUF_SIZE  65536

// GPU buffer types
typedef enum buf_type {
  BT_GLOB = 0,
  BT_MTL,
  BT_INST,
  BT_TLAS_NODE,
  BT_TRI,
  BT_LTRI,
  BT_LNODE,
  BT_INDEX,
  BT_BVH_NODE,
} buf_type;

typedef struct render_data {
  scene     *scene;
  uint16_t  width;
  uint16_t  height;
  uint8_t   spp;
  uint8_t   bounces;
  uint32_t  frame;
  uint32_t  gathered_spp;
} render_data;

#ifndef NATIVE_BUILD

extern void gpu_create_res(uint32_t glob_sz, uint32_t mtl_sz,  uint32_t inst_sz,
    uint32_t tlas_node_sz, uint32_t tri_sz, uint32_t ltri_sz, uint32_t lnode_sz,
    uint32_t index_sz, uint32_t bvh_node_sz);

extern void gpu_write_buf(buf_type type, uint32_t dst_ofs,
    const void *src, uint32_t src_sz);

#else

#define gpu_create_res(...) ((void)0)
#define gpu_write_buf(...) ((void)0)

#endif

uint8_t renderer_gpu_alloc(uint32_t total_tri_cnt, uint32_t total_ltri_cnt,
    uint32_t total_mtl_cnt, uint32_t total_inst_cnt)
{
  // Sanity check for implicitly assumed sizes/counts
  if(sizeof(mtl) > sizeof(inst) || 2 * sizeof(tlas_node) > sizeof(inst) ||
      total_inst_cnt * sizeof(inst) > MAX_UNIFORM_BUF_SIZE || total_mtl_cnt > total_inst_cnt)
    return 1;

  // Adjust max inst count to what the uniform buffer can take.
  // 65536 bytes uniform buf / 80 bytes inst size = 819 insts and is hardcoded in the shader.
  total_inst_cnt = MAX_UNIFORM_BUF_SIZE / sizeof(inst);

  // Each instance can have its own mtl (assuming that inst size is greater/equal than mtl)
  total_mtl_cnt = total_inst_cnt;

  gpu_create_res(
      GLOB_BUF_SIZE, // Globals (uniform buf)
      total_mtl_cnt * sizeof(mtl), // Materials (uniform buf)
      total_inst_cnt * sizeof(inst), // Instances (uniform buf)
      2 * total_inst_cnt * sizeof(tlas_node), // TLAS nodes (storage buf)
      total_tri_cnt * sizeof(tri), // Tris (storage buf)
      total_ltri_cnt * sizeof(ltri), // LTris (storage buf)
      2 * total_ltri_cnt * sizeof(lnode), // Light nodes (storage buf)
      total_tri_cnt * sizeof(uint32_t), // Indices (storage buf)
      2 * total_tri_cnt * sizeof(bvh_node)); // BVH nodes (storage buf)

  return 0;
}

render_data *renderer_init(scene *s, uint16_t width, uint16_t height, uint8_t spp)
{
  render_data *rd = malloc(sizeof(*rd));

  rd->scene = s;
  rd->width = width;
  rd->height = height;
  rd->spp = spp;
  rd->bounces = MAX_BOUNCES;
  rd->frame = 0;
  rd->gathered_spp = 0;

  return rd;
}

void renderer_release(render_data *rd)
{
  free(rd);
}

void reset_samples(render_data *rd)
{
  // Reset gathered samples
  rd->gathered_spp = TEMPORAL_WEIGHT * rd->gathered_spp;
}

void push_mtls(render_data *rd)
{
  scene *s = rd->scene;
  gpu_write_buf(BT_MTL, 0, s->mtls, s->mtl_cnt * sizeof(*s->mtls));
  scene_clr_dirty(s, RT_MTL);
}

void push_ltris_lnodes(render_data *rd)
{
  scene *s = rd->scene;

#ifndef NATIVE_BUILD
  // Push ltris and light nodes
  gpu_write_buf(BT_LTRI, 0, s->ltris, s->ltri_cnt * sizeof(*s->ltris));
  gpu_write_buf(BT_LNODE, 0, s->lnodes, 2 * s->ltri_cnt * sizeof(*s->lnodes));

  // Push tris because their ltri id might have changed
  uint32_t cnt = 0;
  for(uint32_t i=0; i<s->mesh_cnt; i++) {    
    mesh *m = &s->meshes[i];
    gpu_write_buf(BT_TRI, cnt * sizeof(*m->tris), m->tris, m->tri_cnt * sizeof(*m->tris));
    cnt += m->tri_cnt;
  }
#endif

  scene_clr_dirty(s, RT_LTRI);
}

void renderer_update_static(render_data *rd)
{
  scene *s = rd->scene;

#ifndef NATIVE_BUILD
  // Push part of globals
  uint32_t cfg[4] = { rd->width, rd->height, rd->spp, rd->bounces };
  gpu_write_buf(BT_GLOB, GLOB_BUF_OFS_CFG, cfg, sizeof(cfg));
#endif

  // Update instances, build ltris and tlas
  scene_prepare_render(s);

  // Prepare bvhs
  if(s->dirty & RT_MESH) {
#ifndef NATIVE_BUILD
    // Push tris, indices and bvh nodes
    uint32_t cnt = 0;
    for(uint32_t i=0; i<s->mesh_cnt; i++) {    
      mesh *m = &s->meshes[i];
      gpu_write_buf(BT_TRI, cnt * sizeof(*m->tris), m->tris, m->tri_cnt * sizeof(*m->tris));

      bvh *b = &s->bvhs[i];
      gpu_write_buf(BT_INDEX, cnt * sizeof(*b->indices), b->indices, m->tri_cnt * sizeof(*b->indices));
      gpu_write_buf(BT_BVH_NODE, 2 * cnt * sizeof(*b->nodes), b->nodes, 2 * m->tri_cnt * sizeof(*b->nodes));

      cnt += m->tri_cnt;
    }
#endif
    scene_clr_dirty(s, RT_MESH);
  } 
 
  if(s->dirty & RT_MTL)
    push_mtls(rd);

  if(rd->scene->dirty & RT_LTRI)
    push_ltris_lnodes(rd);
}

void renderer_update(render_data *rd, float time)
{
  scene *s = rd->scene;

  // In case something changed, revisit instances, ltris and tlas
  scene_prepare_render(s);

  // Invalidate previous samples
  if(rd->scene->dirty > 0)
    reset_samples(rd);

  // Push camera and view
  if(rd->scene->dirty & RT_CAM_VIEW) {
    cam *cam = scene_get_active_cam(s);
    view_calc(&s->view, rd->width, rd->height, cam);
    gpu_write_buf(BT_GLOB, GLOB_BUF_OFS_CAM, cam, CAM_BUF_SIZE);
    gpu_write_buf(BT_GLOB, GLOB_BUF_OFS_VIEW, &s->view, sizeof(s->view));
    scene_clr_dirty(s, RT_CAM_VIEW);
  }

  // Push materials
  if(rd->scene->dirty & RT_MTL)
    push_mtls(rd);

  // Push ltris
  if(rd->scene->dirty & RT_LTRI)
    push_ltris_lnodes(rd);

  // Push TLAS and instances
  if(rd->scene->dirty & RT_INST) {
    gpu_write_buf(BT_TLAS_NODE, 0, s->tlas_nodes, 2 * s->inst_cnt * sizeof(*s->tlas_nodes));
    gpu_write_buf(BT_INST, 0, s->instances, s->inst_cnt * sizeof(*s->instances));
    scene_clr_dirty(s, RT_INST);
  }

#ifndef NATIVE_BUILD
  // Push frame data
  uint32_t frameUint[4] = { rd->frame, rd->gathered_spp, /* pad */ 0, /* pad */ 0 };
  gpu_write_buf(BT_GLOB, GLOB_BUF_OFS_FRAME, frameUint, sizeof(frameUint));
  float frameFloat[4] = { s->bg_col.x, s->bg_col.y, s->bg_col.z,
    rd->spp / (float)(rd->gathered_spp + rd->spp) };
  gpu_write_buf(BT_GLOB, GLOB_BUF_OFS_FRAME + sizeof(frameUint), frameFloat, sizeof(frameFloat));
#endif

  // Update frame and sample counter
  rd->frame++;
  rd->gathered_spp += rd->spp;
}

#ifdef NATIVE_BUILD

void renderer_render(render_data *rd, SDL_Surface *surface)
{
  float weight = rd->spp / (float)rd->gathered_spp;
#define BLOCK_SIZE 4
  for(uint32_t j=0; j<rd->height; j+=BLOCK_SIZE) {
    for(uint32_t i=0; i<rd->width; i+=BLOCK_SIZE) {
      for(uint32_t y=0; y<BLOCK_SIZE; y++) {
        for(uint32_t x=0; x<BLOCK_SIZE; x++) {
          ray r;
          cam *cam = scene_get_active_cam(rd->scene);
          cam_create_primary_ray(cam, &r, (float)(i + x), (float)(j + y), &rd->scene->view);
          hit h = (hit){ .t = MAX_DISTANCE };
          intersect_tlas(&r, rd->scene->tlas_nodes, rd->scene->instances, rd->scene->bvhs, &h);
          vec3 c = rd->scene->bg_col;
          if(h.t < MAX_DISTANCE) {
            inst *inst = &rd->scene->instances[h.e & INST_ID_MASK];
            vec3 nrm;
            uint16_t mtl_id;
            if(!(inst->data & SHAPE_TYPE_BIT)) {
              // Mesh
              mat4 inv_transform;
              mat4_from_row3x4(inv_transform, inst->inv_transform);
              uint32_t tri_idx = h.e >> 16;
              tri* tri = &rd->scene->meshes[inst->data & MESH_SHAPE_MASK].tris[tri_idx];
              nrm = vec3_add(vec3_add(vec3_scale(tri->n1, h.u), vec3_scale(tri->n2, h.v)), vec3_scale(tri->n0, 1.0f - h.u - h.v));
              mat4 inv_transform_t;
              mat4_transpose(inv_transform_t, inv_transform);
              nrm = vec3_unit(mat4_mul_dir(inv_transform_t, nrm));
              mtl_id = (inst->data & MTL_OVERRIDE_BIT) ? (inst->id >> 16) : (tri->mtl & MTL_ID_MASK);
            } else {
              // Shape
              mat4 inv_transform;
              mat4_from_row3x4(inv_transform, inst->inv_transform);
              mat4 inv_transform_t;
              mat4_transpose(inv_transform_t, inv_transform);
              switch((shape_type)(h.e >> 16)) {
                case ST_PLANE: 
                  nrm = vec3_unit(mat4_mul_dir(inv_transform_t, (vec3){ 0.0f, 1.0f, 0.0f }));
                  break;
                case ST_BOX:
                  {
                    vec3 hit_pos = vec3_add(r.ori, vec3_scale(r.dir, h.t));
                    hit_pos = mat4_mul_pos(inv_transform, hit_pos);
                    nrm = (vec3){ 
                      (float)((int32_t)(hit_pos.x * 1.0001f)),
                      (float)((int32_t)(hit_pos.y * 1.0001f)),
                      (float)((int32_t)(hit_pos.z * 1.0001f)) };
                    nrm = vec3_unit(mat4_mul_dir(inv_transform_t, nrm));
                  }
                  break;
                case ST_SPHERE:
                  {
                    vec3 hit_pos = vec3_add(r.ori, vec3_scale(r.dir, h.t));
                    nrm = vec3_unit(mat4_mul_dir(inv_transform_t, hit_pos));
                  }
                  break;
              }
              mtl_id = inst->id >> 16;
            }
            nrm = vec3_scale(vec3_add(nrm, (vec3){ 1, 1, 1 }), 0.5f);
            mtl *m = &rd->scene->mtls[mtl_id];
            c = m->col;
            //c = vec3_mul(nrm, c); 
            //c = nrm;
          }
          uint32_t index = rd->width * (j + y) + (i + x);
          c = vec3_add(vec3_scale(vec3_uint32(((uint32_t *)surface->pixels)[index]), 1.0f - weight), vec3_scale(c, weight));
          uint32_t cr = min(255, (uint32_t)(255 * c.x));  
          uint32_t cg = min(255, (uint32_t)(255 * c.y));
          uint32_t cb = min(255, (uint32_t)(255 * c.z));
          ((uint32_t *)surface->pixels)[index] = 0xff << 24 | cr << 16 | cg << 8 | cb;
        }
      }
    }
  }
}
#endif
