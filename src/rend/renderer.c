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
#include "../util/vec3.h"

#ifdef NATIVE_BUILD
#include <SDL.h>
#include "../util/ray.h"
#include "intersect.h"
#endif

#define CAM_BUF_SIZE          48
#define MAX_UNIFORM_BUF_SIZE  65536

// GPU buffer types
typedef enum buf_type {
  BT_CAM = 0,
  BT_MTL,
  BT_INST,
  BT_TRI,
  BT_LTRI,
  BT_NODE, // blas + tlas
  // ..more buffers JS-side here..
  BT_CFG = 10
} buf_type;

static uint32_t total_tris = 0;

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

void renderer_update(scene *s, bool converge)
{
  scene_prepare_render(s);

  if(!converge || s->dirty > 0)
    reset_samples();

  if(s->dirty & RT_CFG) {
    gpu_write_buf(BT_CFG, 16 * 4, &s->bg_col, sizeof(vec3));
    scene_clr_dirty(s, RT_CAM);
  }

  if(s->dirty & RT_CAM) {
#ifndef NATIVE_BUILD
    cam *cam = scene_get_active_cam(s);
    float cam_data[12] = {
      cam->eye.x, cam->eye.y, cam->eye.z, tanf(0.5f * cam->vert_fov * PI / 180.0f),
      cam->right.x, cam->right.y, cam->right.z, cam->foc_dist,
      cam->up.x, cam->up.y, cam->up.z, tanf(0.5f * cam->foc_angle * PI / 180.0f) };
    gpu_write_buf(BT_CAM, 0, cam_data, sizeof(cam_data));
#endif
    scene_clr_dirty(s, RT_CAM);
  }

  if(s->dirty & RT_MTL) {
    gpu_write_buf(BT_MTL, 0, s->mtls, s->mtl_cnt * sizeof(*s->mtls));
    scene_clr_dirty(s, RT_MTL);
  }

  if(s->dirty & RT_TRI) {
#ifndef NATIVE_BUILD
    for(uint32_t i=0; i<s->mesh_cnt; i++) {
      mesh *m = &s->meshes[i];
      gpu_write_buf(BT_TRI, m->ofs * sizeof(*m->tris), m->tris, m->tri_cnt * sizeof(*m->tris));
    }
#endif
    scene_clr_dirty(s, RT_TRI);
  }

  if(s->dirty & RT_LTRI) {
    gpu_write_buf(BT_LTRI, 0, s->ltris, s->ltri_cnt * sizeof(*s->ltris));
    scene_clr_dirty(s, RT_LTRI);
  }

  if(s->dirty & RT_BLAS) {
    gpu_write_buf(BT_NODE, 0, s->blas_nodes, 2 * s->max_tri_cnt * sizeof(*s->blas_nodes));
    scene_clr_dirty(s, RT_BLAS);
  }

  if(s->dirty & RT_INST) {
    gpu_write_buf(BT_INST, 0, s->instances, s->inst_cnt * sizeof(*s->instances));
    gpu_write_buf(BT_NODE, 2 * total_tris * sizeof(*s->blas_nodes), s->tlas_nodes, 2 * s->inst_cnt * sizeof(*s->tlas_nodes));
    scene_clr_dirty(s, RT_INST);
  }
}

#ifdef NATIVE_BUILD

typedef struct view {
  vec3      pix_delta_x;
  vec3      pix_delta_y;
  vec3      pix_top_left;
} view;

void calc_view(view *v, float width, float height, const cam *c)
{
  float v_height = 2.0f * tanf(0.5f * c->vert_fov * PI / 180.0f) * c->foc_dist;
  float v_width = v_height * width / height;

  vec3 v_right = vec3_scale(c->right, v_width);
  vec3 v_down = vec3_scale(c->up, -v_height);

  v->pix_delta_x = vec3_scale(v_right, 1.0f / width);
  v->pix_delta_y = vec3_scale(v_down, 1.0f / height);

  // viewport_top_left = eye - foc_dist * fwd - 0.5 * (viewport_right + viewport_down);
  vec3 v_top_left = vec3_add(c->eye,
      vec3_add(
        vec3_scale(c->fwd, -c->foc_dist),
        vec3_scale(vec3_add(v_right, v_down), -0.5f)));

  // pixel_top_left = viewport_top_left + 0.5 * (pixel_delta_x + pixel_delta_y)
  v->pix_top_left = vec3_add(v_top_left,
      vec3_scale(vec3_add(v->pix_delta_x, v->pix_delta_y), 0.5f));
}

void create_primary_ray(ray *ray, float x, float y, const cam *c, const view *v)
{
  // Viewplane pixel position
  vec3 pix_smpl = vec3_add(v->pix_top_left, vec3_add(
        vec3_scale(v->pix_delta_x, x), vec3_scale(v->pix_delta_y, y)));

  // Jitter viewplane position (AA)
  pix_smpl = vec3_add(pix_smpl, vec3_add(
        vec3_scale(v->pix_delta_x, pcg_randf() - 0.5f),
        vec3_scale(v->pix_delta_y, pcg_randf() - 0.5f)));

  // Jitter eye (DOF)
  vec3 eye_smpl = c->eye;
  if(c->foc_angle > 0.0f) {
    float foc_rad = c->foc_dist * tanf(0.5f * c->foc_angle * PI / 180.0f);
    vec3  disk_smpl = vec3_rand2_disk();
    eye_smpl = vec3_add(eye_smpl,
        vec3_scale(vec3_add(
            vec3_scale(c->right, disk_smpl.x),
            vec3_scale(c->up, disk_smpl.y)),
          foc_rad));
  }

  ray_create(ray, eye_smpl, vec3_unit(vec3_sub(pix_smpl, eye_smpl)));
}

void renderer_render(SDL_Surface *screen, scene *s)
{
#define BLOCK_SIZE 4
  view v;
  cam *cam = scene_get_active_cam(s);
  calc_view(&v, screen->w, screen->h, cam);
  for(uint32_t j=0; j<(uint32_t)screen->h; j+=BLOCK_SIZE) {
    for(uint32_t i=0; i<(uint32_t)screen->w; i+=BLOCK_SIZE) {
      for(uint32_t y=0; y<BLOCK_SIZE; y++) {
        for(uint32_t x=0; x<BLOCK_SIZE; x++) {
          ray r;
          create_primary_ray(&r, (float)(i + x), (float)(j + y), cam, &v);
          hit h = (hit){ .t = MAX_DISTANCE };
          intersect_tlas(&r, s->tlas_nodes, s->instances, s->meshes, s->blas_nodes, &h);
          vec3 c = s->bg_col;
          if(h.t < MAX_DISTANCE) {
            inst *inst = &s->instances[h.e & INST_ID_MASK];
            vec3 nrm;
            uint16_t mtl_id;
            if(!(inst->data & SHAPE_TYPE_BIT)) {
              // Mesh
              mat4 inv_transform;
              mat4_from_row3x4(inv_transform, inst->inv_transform);
              uint32_t tri_idx = h.e >> 16;
              tri* tri = &s->meshes[inst->data & MESH_SHAPE_MASK].tris[tri_idx];
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
            mtl *m = &s->mtls[mtl_id];
            c = m->col;
            //c = vec3_mul(nrm, c);
            //c = nrm;
          }
          uint32_t cr = min(255, (uint32_t)(255 * c.x));
          uint32_t cg = min(255, (uint32_t)(255 * c.y));
          uint32_t cb = min(255, (uint32_t)(255 * c.z));
          ((uint32_t *)screen->pixels)[screen->w * (j + y) + (i + x)] = 0xff << 24 | cr << 16 | cg << 8 | cb;
        }
      }
    }
  }
}

#endif
