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

// Global buffer size and offsets
#define GLOB_BUF_SZ         144
#define GLOB_BUF_OFS_CFG    0
#define GLOB_BUF_OFS_FRAME  16
#define GLOB_BUF_OFS_CAM    48
#define GLOB_BUF_OFS_VIEW   96

typedef enum buf_type {
  BT_GLOB = 0,
  BT_TRI,
  BT_INDEX,
  BT_BVH_NODE,
  BT_TLAS_NODE,
  BT_INST,
  BT_MTL
} buf_type;

typedef struct render_data {
  scene     *scene;
  uint16_t  width;
  uint16_t  height;
  uint8_t   spp;
  uint8_t   bounces;
  uint32_t  gathered_spp;
  vec3      bg_col;
} render_data;

extern void gpu_create_res(uint32_t glob_sz, uint32_t tri_sz,
    uint32_t index_sz, uint32_t bvh_node_sz, uint32_t tlas_node_sz, uint32_t inst_sz,
    uint32_t mtl_sz);

extern void gpu_write_buf(buf_type type, uint32_t dst_ofs,
    const void *src, uint32_t src_sz);

render_data *renderer_init(scene *s, uint16_t width, uint16_t height, uint8_t spp)
{
  render_data *rd = malloc(sizeof(*rd));

  rd->scene = s;
  rd->width = width;
  rd->height = height;
  rd->spp = spp;
  rd->bounces = 5; // TODO RR
  rd->gathered_spp = 0;

  // Calc total number of tris in scene
  uint32_t total_tri_cnt = 0;
  for(uint32_t i=0; i<s->mesh_cnt; i++)
    total_tri_cnt += s->meshes[i].tri_cnt;

  // Create GPU buffer
  gpu_create_res(GLOB_BUF_SZ, total_tri_cnt * sizeof(tri), total_tri_cnt * sizeof(uint32_t),
      2 * total_tri_cnt * sizeof(bvh_node), (2 * s->inst_cnt + 1) * sizeof(tlas_node),
      s->inst_cnt * sizeof(inst), s->mtl_cnt * sizeof(mtl));

  return rd;
}

void renderer_release(render_data *rd)
{
  free(rd);
}

void reset_samples(render_data *rd)
{
  rd->gathered_spp = 0; // TEMPORAL_WEIGHT * rd->spp;
}

void renderer_set_bg_col(render_data *rd, vec3 bg_col)
{
  memcpy(&rd->bg_col, &bg_col, sizeof(rd->bg_col));
  reset_samples(rd);
}

void renderer_push_static(render_data *rd)
{
  scene *s = rd->scene;
  
  // Push part of globals
  uint32_t cfg[4] = { rd->width, rd->height, rd->spp, rd->bounces };
  gpu_write_buf(BT_GLOB, GLOB_BUF_OFS_CFG, cfg, sizeof(cfg));

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

  // Push mtls
  gpu_write_buf(BT_MTL, 0, s->mtls, s->mtl_cnt * sizeof(*s->mtls));

  scene_unset_dirty(s, RT_MESH | RT_MTL);
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
    scene_unset_dirty(s, RT_CAM_VIEW);
  }

  // Push materials
  if(rd->scene->dirty & RT_MTL) {
    gpu_write_buf(BT_MTL, 0, rd->scene->mtls, s->mtl_cnt * sizeof(*s->mtls));
    scene_unset_dirty(s, RT_MTL);
  }

  // Push TLAS and instances
  if(rd->scene->dirty & RT_INST) {
    gpu_write_buf(BT_TLAS_NODE, 0, s->tlas_nodes, (2 * s->inst_cnt + 1) * sizeof(*s->tlas_nodes));
    gpu_write_buf(BT_INST, 0, s->instances, s->inst_cnt * sizeof(*s->instances));
    scene_unset_dirty(s, RT_INST);
  }

  // Push frame data
  float frame[8] = { pcg_randf(), rd->spp / (float)(rd->gathered_spp + rd->spp),
    time, 0.0f, rd->bg_col.x, rd->bg_col.y, rd->bg_col.z, 0.0f };
  gpu_write_buf(BT_GLOB, GLOB_BUF_OFS_FRAME, frame, sizeof(frame));

  rd->gathered_spp += rd->spp;
}
