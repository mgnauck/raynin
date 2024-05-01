#include "renderer.h"
#include "bvh.h"
#include "inst.h"
#include "mesh.h"
#include "mtl.h"
#include "mutil.h"
#include "scene.h"
#include "settings.h"
#include "sutil.h"
#include "tlas.h"
#include "tri.h"

#ifdef NATIVE_BUILD
#include <SDL.h>
#include "ray.h"
#include "intersect.h"
#endif

// Global buffer offsets
#define GLOB_BUF_OFS_CFG    0
#define GLOB_BUF_OFS_FRAME  16
#define GLOB_BUF_OFS_CAM    48
#define GLOB_BUF_OFS_VIEW   96
#define GLOB_BUF_OFS_SEQ    144

#define MAX_BOUNCES         5
#define SEQ_DIM             16 // Dimensionality of a path/sequence
#define SEQ_LEN             MAX_BOUNCES * SEQ_DIM

// GPU buffer types
typedef enum buf_type {
  BT_GLOB = 0,
  BT_TRI,
  BT_LTRI,
  BT_INDEX,
  BT_BVH_NODE,
  BT_TLAS_NODE,
  BT_INST,
  BT_MTL,
} buf_type;

typedef struct render_data {
  scene     *scene;
  uint16_t  width;
  uint16_t  height;
  uint8_t   spp;
  uint8_t   bounces;
  uint32_t  frame;
  uint32_t  gathered_spp;
  vec3      bg_col;
  float     *alpha;
} render_data;

#ifndef NATIVE_BUILD

extern void gpu_create_res(uint32_t glob_sz, uint32_t tri_sz, uint32_t ltri_sz,
    uint32_t index_sz, uint32_t bvh_node_sz, uint32_t tlas_node_sz, uint32_t inst_sz,
    uint32_t mtl_sz);

extern void gpu_write_buf(buf_type type, uint32_t dst_ofs,
    const void *src, uint32_t src_sz);

#else

#define gpu_create_res(...) ((void)0)
#define gpu_write_buf(...) ((void)0)

#endif

void renderer_gpu_alloc(uint32_t total_tri_cnt, uint32_t total_ltri_cnt,
    uint32_t total_mtl_cnt, uint32_t total_inst_cnt)
{
  gpu_create_res(
      GLOB_BUF_OFS_SEQ + SEQ_LEN * sizeof(float), // Globals + quasirandom sequence (uniform buf))
      total_tri_cnt * sizeof(tri), // Tris
      total_ltri_cnt * sizeof(ltri), // LTris
      total_tri_cnt * sizeof(uint32_t), // Indices
      2 * total_tri_cnt * sizeof(bvh_node), // BVH nodes
      2 * total_inst_cnt * sizeof(tlas_node), // TLAS nodes
      total_inst_cnt * sizeof(inst), // Instances
      total_mtl_cnt * sizeof(mtl)); // Materials
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
  rd->bg_col = (vec3){ 0.0f, 0.0f, 0.0f };
  rd->alpha = malloc(SEQ_LEN * sizeof(*rd->alpha));

  // Initialize alphas for quasirandom sequence (low discrepancy series)
  qrand_alpha(SEQ_LEN, rd->alpha);

  return rd;
}

void renderer_release(render_data *rd)
{
  free(rd->alpha);
  free(rd);
}

void reset_samples(render_data *rd)
{
  // Reset gathered samples
  rd->gathered_spp = TEMPORAL_WEIGHT * rd->gathered_spp;
}

void renderer_set_bg_col(render_data *rd, vec3 bg_col)
{
  memcpy(&rd->bg_col, &bg_col, sizeof(rd->bg_col));
  scene_set_dirty(rd->scene, RT_CAM_VIEW);
}

void push_mtls(render_data *rd)
{
  scene *s = rd->scene;
  gpu_write_buf(BT_MTL, 0, s->mtls, s->mtl_cnt * sizeof(*s->mtls));
  scene_clr_dirty(s, RT_MTL);
}

void push_ltris(render_data *rd)
{
  scene *s = rd->scene;
  gpu_write_buf(BT_LTRI, 0, s->ltris, s->ltri_cnt * sizeof(*s->ltris));
  scene_clr_dirty(s, RT_LTRI);
}

void renderer_update_static(render_data *rd)
{
  scene *s = rd->scene;

#ifndef NATIVE_BUILD
  // Push part of globals
  uint32_t cfg[4] = { rd->width, rd->height, rd->spp, rd->bounces };
  gpu_write_buf(BT_GLOB, GLOB_BUF_OFS_CFG, cfg, sizeof(cfg));

  // Push alphas of quasi random sequence
  gpu_write_buf(BT_GLOB, GLOB_BUF_OFS_SEQ, rd->alpha, SEQ_LEN * sizeof(*rd->alpha));
#endif

  if(s->dirty & RT_MESH) {
    scene_build_bvhs(s);

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
    push_ltris(rd);
}

void renderer_update(render_data *rd, float time)
{
  scene *s = rd->scene;

  // Finalize instances and TLAS
  scene_prepare_render(s);

  // Invalidate previous samples
  if(rd->scene->dirty > 0)
    reset_samples(rd);

  // Push camera and view
  if(rd->scene->dirty & RT_CAM_VIEW) {
    view_calc(&s->view, rd->width, rd->height, &s->cam);
    gpu_write_buf(BT_GLOB, GLOB_BUF_OFS_CAM, &s->cam, CAM_BUF_SIZE);
    gpu_write_buf(BT_GLOB, GLOB_BUF_OFS_VIEW, &s->view, sizeof(s->view));
    scene_clr_dirty(s, RT_CAM_VIEW);
  }

  // Push materials
  if(rd->scene->dirty & RT_MTL)
    push_mtls(rd);

  // Push ltris
  if(rd->scene->dirty & RT_LTRI)
    push_ltris(rd);

  // Push TLAS and instances
  if(rd->scene->dirty & RT_INST) {
    gpu_write_buf(BT_TLAS_NODE, 0, s->tlas_nodes, 2 * s->inst_cnt * sizeof(*s->tlas_nodes));
    gpu_write_buf(BT_INST, 0, s->instances, s->inst_cnt * sizeof(*s->instances));
    scene_clr_dirty(s, RT_INST);
  }

#ifndef NATIVE_BUILD
  // Push frame data
  float frame[8] = { pcg_randf(), pcg_randf(),
    (float)rd->gathered_spp, rd->spp / (float)(rd->gathered_spp + rd->spp),
    rd->bg_col.x, rd->bg_col.y, rd->bg_col.z, (float)rd->frame };
  gpu_write_buf(BT_GLOB, GLOB_BUF_OFS_FRAME, frame, sizeof(frame));
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
          ray_create_primary(&r, (float)(i + x), (float)(j + y), &rd->scene->view, &rd->scene->cam);
          hit h = (hit){ .t = MAX_DISTANCE };
          intersect_tlas(&r, rd->scene->tlas_nodes, rd->scene->instances, rd->scene->bvhs, &h);
          vec3 c = rd->bg_col;
          if(h.t < MAX_DISTANCE) {
            inst *inst = &rd->scene->instances[h.e & INST_ID_MASK];
            mat4 transform;
            mat4_from_row3x4(transform, inst->transform);
            vec3 nrm;
            uint16_t mtl_id;
            if(!(inst->data & SHAPE_TYPE_BIT)) {
              // Mesh
              uint32_t tri_idx = h.e >> 16;
              tri* tri = &rd->scene->meshes[inst->data & MESH_SHAPE_MASK].tris[tri_idx];
              nrm = vec3_add(vec3_add(vec3_scale(tri->n1, h.u), vec3_scale(tri->n2, h.v)), vec3_scale(tri->n0, 1.0f - h.u - h.v));
              nrm = vec3_unit(mat4_mul_dir(transform, nrm));
              mtl_id = (inst->data & MTL_OVERRIDE_BIT) ? (inst->id >> 16) : (tri->mtl & MTL_ID_MASK);
            } else {
              // Shape
              switch((shape_type)(h.e >> 16)) {
                case ST_PLANE: 
                  nrm = vec3_unit(mat4_mul_dir(transform, (vec3){ 0.0f, 1.0f, 0.0f }));
                  break;
                case ST_BOX:
                  {
                    vec3 hit_pos = vec3_add(r.ori, vec3_scale(r.dir, h.t));
                    mat4 inv_transform;
                    mat4_from_row3x4(inv_transform, inst->inv_transform);
                    hit_pos = mat4_mul_pos(inv_transform, hit_pos);
                    nrm = (vec3){ 
                      (float)((int32_t)(hit_pos.x * 1.0001f)),
                      (float)((int32_t)(hit_pos.y * 1.0001f)),
                      (float)((int32_t)(hit_pos.z * 1.0001f)) };
                    nrm = vec3_unit(mat4_mul_dir(transform, nrm));
                  }
                  break;
                case ST_SPHERE:
                  {
                    vec3 hit_pos = vec3_add(r.ori, vec3_scale(r.dir, h.t));
                    nrm = vec3_unit(vec3_sub(hit_pos, mat4_get_trans(transform)));
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
