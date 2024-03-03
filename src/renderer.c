#include "renderer.h"
#include <stdint.h>
#include "scene.h"
#include "settings.h"

#define GLOB_BUF_SIZE       144
#define GLOB_BUF_OFS_CFG    0
#define GLOB_BUF_OFS_FRAME  16
#define GLOB_BUF_OFS_CAM    48
#define GLOB_BUF_OFS_VIEW   96

typedef enum buf_type {
  GLOB = 0,
  TRI,
  INDEX,
  BVH_NODE,
  TLAS_NODE,
  INST,
  MTL
} buf_type;

typedef struct render_data {
  scene     *scene;
  uint16_t  width;
  uint16_t  height;
  uint8_t   spp;
  uint8_t   bounces;
  uint32_t  gathered_samples;
  vec3      bg_col;
  uint32_t  dirty;
  uint32_t  glob_sz;
  uint32_t  mtl_sz;
  uint32_t  tri_sz;
  uint32_t  bvh_node_sz;
  uint32_t  index_sz;
  uint32_t  tlas_node_size;
  uint32_t  inst_sz;
} render_data;

extern void gpu_create_res(uint32_t glob_sz, uint32_t tri_sz,
    uint32_t index_sz, uint32_t bvh_node_sz, uint32_t tlas_node_sz, uint32_t inst_sz,
    uint32_t mtl_sz);

extern void gpu_write_buf(buf_type src_type, uint32_t dest_ofs,
    const void *src, uint32_t size);

render_data *renderer_init(scene *s, uint16_t width, uint16_t height, uint8_t spp)
{
  render_data *rd = malloc(sizeof(*rd));

  rd->scene = s;
  rd->width = width;
  rd->height = height;
  rd->spp = spp;
  rd->bounces = 5; // TODO RR

  rd->cam_view_dirty = true;

  // Eval buf sizes
  rd->glob_sz = GLOB_BUF_SIZE;
  // ..

  // Create GPU buffer
  //gpu_create_res(..);

  return rd;
}

void renderer_release(render_data *rd)
{
  free(rd);
}

void renderer_set_dirty(render_data *rd, res_type r)
{
  rd->dirty |= r;
}

void renderer_set_bg_col(render_data *rd, vec3 bg_col)
{
  memcpy(&rd->bg_col, &bg_col, sizeof(rd->bg_col));
  renderer_dirty(rd, SAMPLES);
}

void renderer_push_static_res(render_data *rd)
{
  uint32_t cfg[4] = { rd->width, rd->height, rd->spp, rd->bounces };
  gpu_write_buf(GLOB, GLOB_BUF_OFS_CFG, cfg, 4 * sizeof(cfg));

  // Write all static data buffer
  //gpu_write_buf(TRI, 0, buf_ptr(TRI, 0), buf_len(TRI));
  //gpu_write_buf(INDEX, 0, buf_ptr(INDEX, 0), buf_len(INDEX));
  //gpu_write_buf(BVH_NODE, 0, buf_ptr(BVH_NODE, 0), buf_len(BVH_NODE));
  //gpu_write_buf(MTL, 0, buf_ptr(MTL, 0), buf_len(MTL));
}

void renderer_update(render_data *rd, float time)
{
  // Finalize instances and TLAS
  scene_prepare_render(rd->scene);

  // Invalidate previous samples
  if(rd->dirty > 0 || rd->scene->dirty & res_type.INST)
    rd->gathered_samples = TEMPORAL_WEIGHT * rd->spp;

  // Update camera and view
  if(rd->dirty & CAM_VIEW) {
    view_calc(&rd->scene->view, rd->width, rd->height, &rd->scene->cam);
    gpu_write_buf(GLOB, GLOB_BUF_OFS_CAM, &rd->scene->cam, CAM_BUF_SIZE);
    gpu_write_buf(GLOB, GLOB_BUF_OFS_VIEW, &rd->scene->view, sizeof(view));
  }

  // Write tlas and instance buffer
  //gpu_write_buf(TLAS_NODE, 0, buf_ptr(TLAS_NODE, 0), buf_len(TLAS_NODE));
  //gpu_write_buf(INST, 0, buf_ptr(INST, 0), buf_len(INST));

  // Push frame data
  float frame[8] = { pcg_randf(), rd->spp / (float)(rd->gathered_samples + rd->spp),
    time, 0.0f, rd->bg_col.x, rd->bg_col.y, rd->bg_col.z, 0.0f };
  gpu_write_buf(GLOB, GLOB_BUF_OFS_FRAME, frame, 8 * sizeof(frame));

  // Update sample count and dirty flag
  rd->gathered_samples += rd->spp;
  rd->dirty = 0;
}
