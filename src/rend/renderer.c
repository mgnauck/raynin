#include "renderer.h"

#include "../base/math.h"
#include "../base/string.h"
#include "../base/walloc.h"
#include "../scene/cam.h"
#include "../scene/inst.h"
#include "../scene/mesh.h"
#include "../scene/mtl.h"
#include "../scene/scene.h"
#include "../scene/tri.h"
#include "../util/vec3.h"
#include "bvh.h"
#include "postparams.h"

#define CAM_BUF_SIZE         48
#define MAX_UNIFORM_BUF_SIZE 65536

// GPU buffer types
typedef enum buf_type {
  BT_CAM = 0,
  BT_MTL,
  BT_INST,
  BT_TRI,
  BT_NRM,
  BT_LTRI,
  BT_NODE, // blas + tlas
  // ..more buffers on JS-side here..
  BT_CFG = 21,
  BT_POST = 22,
} buf_type;

static uint32_t total_tris = 0;

extern void gpu_create_res(uint32_t glob_sz, uint32_t mtl_sz, uint32_t inst_sz,
                           uint32_t tri_sz, uint32_t nrm_sz, uint32_t ltri_sz,
                           uint32_t node_sz);

extern void gpu_write_buf(buf_type type, uint32_t dst_ofs, const void *src,
                          uint32_t src_sz);

extern void reset_samples();

uint8_t renderer_gpu_alloc(uint32_t total_tri_cnt, uint32_t total_ltri_cnt,
                           uint32_t total_mtl_cnt, uint32_t total_inst_cnt)
{
  // Sanity check for implicitly assumed sizes/counts
  if(sizeof(mtl) > sizeof(inst) || total_mtl_cnt > total_inst_cnt ||
     total_inst_cnt * sizeof(inst) > MAX_UNIFORM_BUF_SIZE)
    return 1;

  // Adjust max inst count to what the uniform buffer can take.
  // 65536 bytes uniform buf / 64 bytes inst size = 1024 insts and is hardcoded
  // in the shader.
  total_inst_cnt = MAX_UNIFORM_BUF_SIZE / sizeof(inst);

  // Each instance can have its own mtl (assuming that inst size is
  // greater/equal than mtl)
  total_mtl_cnt = total_inst_cnt;

  // TODO Define size of post param buffer from WASM instead of fixed in JS

  gpu_create_res(CAM_BUF_SIZE,                    // Camera (uniform buf)
                 total_mtl_cnt * sizeof(mtl),     // Materials (uniform buf)
                 total_inst_cnt * sizeof(inst),   // Instances (uniform buf)
                 total_tri_cnt * sizeof(tri),     // Tris (storage buf)
                 total_tri_cnt * sizeof(tri_nrm), // Tri nrms (storage buf)
                 total_ltri_cnt * sizeof(ltri),   // LTris (storage buf)
                 2 * (total_tri_cnt + total_inst_cnt) *
                     sizeof(node)); // BLAS + TLAS nodes (storage buf)

  total_tris = total_tri_cnt;

  return 0;
}

void renderer_update(scene *s, const post_params *p, bool converge)
{
  scene_prepare_render(s);

  if(!converge || s->dirty > 0 || p)
    reset_samples();

  if(s->dirty & RT_CFG) {
    gpu_write_buf(BT_CFG, 12 * 4, &s->bg_col, sizeof(vec3));
    scene_clr_dirty(s, RT_CFG);
  }

  if(p)
    gpu_write_buf(BT_POST, 0, p, sizeof(*p));

  if(s->dirty & RT_CAM) {
    cam *cam = scene_get_active_cam(s);
    float cam_data[12] = {
        cam->eye.x,   cam->eye.y,
        cam->eye.z,   tanf(0.5f * cam->vert_fov * PI / 180.0f),
        cam->right.x, cam->right.y,
        cam->right.z, cam->foc_dist,
        cam->up.x,    cam->up.y,
        cam->up.z,    tanf(0.5f * cam->foc_angle * PI / 180.0f)};
    gpu_write_buf(BT_CAM, 0, cam_data, sizeof(cam_data));
    scene_clr_dirty(s, RT_CAM);
  }

  if(s->dirty & RT_MTL) {
    gpu_write_buf(BT_MTL, 0, s->mtls, s->mtl_cnt * sizeof(*s->mtls));
    scene_clr_dirty(s, RT_MTL);
  }

  if(s->dirty & RT_TRI) {
    for(uint16_t i = 0; i < s->mesh_cnt; i++) {
      mesh *m = &s->meshes[i];
      gpu_write_buf(BT_TRI, m->ofs * sizeof(*m->tris), m->tris,
                    m->tri_cnt * sizeof(*m->tris));
      gpu_write_buf(BT_NRM, m->ofs * sizeof(*m->tri_nrms), m->tri_nrms,
                    m->tri_cnt * sizeof(*m->tri_nrms));
    }
    scene_clr_dirty(s, RT_TRI);
  }

  if(s->dirty & RT_LTRI) {
    gpu_write_buf(BT_LTRI, 0, s->ltris, s->ltri_cnt * sizeof(*s->ltris));
    scene_clr_dirty(s, RT_LTRI);
  }

  if(s->dirty & RT_BLAS) {
    gpu_write_buf(BT_NODE, 0, s->blas_nodes,
                  2 * s->max_tri_cnt * sizeof(*s->blas_nodes));
    scene_clr_dirty(s, RT_BLAS);
  }

  if(s->dirty & RT_INST) {
    gpu_write_buf(BT_INST, 0, s->instances,
                  s->inst_cnt * sizeof(*s->instances));
    gpu_write_buf(BT_NODE, 2 * total_tris * sizeof(*s->blas_nodes),
                  s->tlas_nodes, 2 * s->inst_cnt * sizeof(*s->tlas_nodes));
    scene_clr_dirty(s, RT_INST);
  }
}
