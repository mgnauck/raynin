#include "renderer.h"
#include "../acc/bvh.h"
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

// Camera uniform buffer offsets
#define CAM_BUF_OFS_CAM      16
#define CAM_BUF_OFS_VIEW     64
#define CAM_BUF_SIZE         112

#define MAX_UNIFORM_BUF_SIZE  65536

// GPU buffer types
typedef enum buf_type {
  BT_CAM = 0,
  BT_MTL,
  BT_INST,
  BT_TRI,
  BT_LTRI,
  BT_NODE, // blas + tlas
} buf_type;

static uint32_t total_tris = 0;

typedef struct render_data {
  scene     *scene;
  uint16_t  width;
  uint16_t  height;
  uint8_t   bounces;
} render_data;

#ifndef NATIVE_BUILD

extern void gpu_create_res(uint32_t glob_sz, uint32_t mtl_sz,
    uint32_t inst_sz, uint32_t tri_sz, uint32_t ltri_sz,
    uint32_t node_sz);

extern void gpu_write_buf(buf_type type, uint32_t dst_ofs,
    const void *src, uint32_t src_sz);

extern void reset_samples();

#else

#define gpu_create_res(...) ((void)0)
#define gpu_write_buf(...) ((void)0)
#define reset_samples(...) ((void)0)

#endif

uint8_t renderer_gpu_alloc(uint32_t total_tri_cnt, uint32_t total_ltri_cnt,
    uint32_t total_mtl_cnt, uint32_t total_inst_cnt)
{
  // Sanity check for implicitly assumed sizes/counts
  if(sizeof(mtl) > sizeof(inst) || 2 * sizeof(node) > sizeof(inst) ||
      total_inst_cnt * sizeof(inst) > MAX_UNIFORM_BUF_SIZE || total_mtl_cnt > total_inst_cnt)
    return 1;

  // Adjust max inst count to what the uniform buffer can take.
  // 65536 bytes uniform buf / 64 bytes inst size = 1024 insts and is hardcoded in the shader.
  total_inst_cnt = MAX_UNIFORM_BUF_SIZE / sizeof(inst);

  // Each instance can have its own mtl (assuming that inst size is greater/equal than mtl)
  total_mtl_cnt = total_inst_cnt;

  gpu_create_res(
      CAM_BUF_SIZE, // Camera (uniform buf)
      total_mtl_cnt * sizeof(mtl), // Materials (uniform buf)
      total_inst_cnt * sizeof(inst), // Instances (uniform buf)
      total_tri_cnt * sizeof(tri), // Tris (storage buf)
      total_ltri_cnt * sizeof(ltri), // LTris (storage buf)
      2 * (total_tri_cnt + total_inst_cnt) * sizeof(node)); // BLAS + TLAS nodes (storage buf)

  total_tris = total_tri_cnt;

  return 0;
}

render_data *renderer_init(scene *s, uint16_t width, uint16_t height, uint32_t max_bounces)
{
  render_data *rd = malloc(sizeof(*rd));

  rd->scene = s;
  rd->width = width;
  rd->height = height;
  rd->bounces = max_bounces;

  return rd;
}

void renderer_release(render_data *rd)
{
  free(rd);
}

void push_cfg(scene *s)
{
  uint32_t cfg[4] = { s->bg_col.x, s->bg_col.y, s->bg_col.z, 2 * total_tris };
  gpu_write_buf(BT_CAM, 0, cfg, sizeof(cfg));
}

void push_cam_view(scene *s, uint32_t width, uint32_t height)
{
  cam *cam = scene_get_active_cam(s);
  gpu_write_buf(BT_CAM, CAM_BUF_OFS_CAM, cam, CAM_SIZE);

  view_calc(&s->view, width, height, cam);
  gpu_write_buf(BT_CAM, CAM_BUF_OFS_VIEW, &s->view, sizeof(s->view));

  scene_clr_dirty(s, RT_CAM_VIEW);
}

void push_mtls(scene *s)
{
  gpu_write_buf(BT_MTL, 0, s->mtls, s->mtl_cnt * sizeof(*s->mtls));

  scene_clr_dirty(s, RT_MTL);
}

void push_tris(scene *s)
{
#ifndef NATIVE_BUILD
  for(uint32_t i=0; i<s->mesh_cnt; i++) {
    mesh *m = &s->meshes[i];
    gpu_write_buf(BT_TRI, m->ofs * sizeof(*m->tris), m->tris, m->tri_cnt * sizeof(*m->tris));
  }
#endif

  scene_clr_dirty(s, RT_TRI);
}

void push_ltris(scene *s)
{
  gpu_write_buf(BT_LTRI, 0, s->ltris, s->ltri_cnt * sizeof(*s->ltris));

  scene_clr_dirty(s, RT_LTRI);
}

void push_blas(scene *s)
{
  gpu_write_buf(BT_NODE, 0, s->blas_nodes, 2 * s->max_tri_cnt * sizeof(*s->blas_nodes));

  scene_clr_dirty(s, RT_MESH);
}

void push_tlas(scene *s)
{
  gpu_write_buf(BT_INST, 0, s->instances, s->inst_cnt * sizeof(*s->instances));
  gpu_write_buf(BT_NODE, 2 * total_tris * sizeof(*s->blas_nodes), s->tlas_nodes, 2 * s->inst_cnt * sizeof(*s->tlas_nodes));

  scene_clr_dirty(s, RT_INST);
}

void renderer_setup(render_data *rd, uint32_t spp)
{
  scene *s = rd->scene;

  scene_prepare_render(s);

  push_cfg(s);
  push_cam_view(s, rd->width, rd->height);
  push_mtls(s);
  push_tris(s);
  push_ltris(s);
  push_blas(s);
  push_tlas(s);
}

void renderer_update(render_data *rd, uint32_t spp, bool converge)
{
  scene *s = rd->scene;

  scene_prepare_render(s);

  if(!converge || rd->scene->dirty > 0)
    reset_samples();

  if(rd->scene->dirty & RT_CAM_VIEW)
    push_cam_view(s, rd->width, rd->height);

  if(rd->scene->dirty & RT_MTL)
    push_mtls(s);

  if(rd->scene->dirty & RT_LTRI) {
    push_ltris(s);
    if(rd->scene->dirty & RT_TRI) // Ltri id in tri is modified by ltri rebuild
      push_tris(s);
  }

  if(rd->scene->dirty & RT_INST)
    push_tlas(s);
}

#ifdef NATIVE_BUILD
void renderer_render(render_data *rd, SDL_Surface *surface)
{
#define BLOCK_SIZE 4
  for(uint32_t j=0; j<rd->height; j+=BLOCK_SIZE) {
    for(uint32_t i=0; i<rd->width; i+=BLOCK_SIZE) {
      for(uint32_t y=0; y<BLOCK_SIZE; y++) {
        for(uint32_t x=0; x<BLOCK_SIZE; x++) {
          ray r;
          cam *cam = scene_get_active_cam(rd->scene);
          cam_create_primary_ray(cam, &r, (float)(i + x), (float)(j + y), &rd->scene->view);
          hit h = (hit){ .t = MAX_DISTANCE };
          intersect_tlas(&r, rd->scene->tlas_nodes, rd->scene->instances, rd->scene->meshes, rd->scene->blas_nodes, &h);
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
          uint32_t cr = min(255, (uint32_t)(255 * c.x));
          uint32_t cg = min(255, (uint32_t)(255 * c.y));
          uint32_t cb = min(255, (uint32_t)(255 * c.z));
          ((uint32_t *)surface->pixels)[rd->width * (j + y) + (i + x)] = 0xff << 24 | cr << 16 | cg << 8 | cb;
        }
      }
    }
  }
}
#endif
