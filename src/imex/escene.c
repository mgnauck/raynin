#include "escene.h"

#include "../base/string.h"
#include "../base/walloc.h"
#include "../scene/mtl.h"
#include "ecam.h"
#include "einst.h"
#include "emesh.h"

void escene_init(escene *s, uint16_t max_mesh_cnt, uint16_t max_mtl_cnt,
                 uint16_t max_cam_cnt, uint16_t max_inst_cnt)
{
  s->mtls = malloc(max_mtl_cnt * sizeof(*s->mtls));
  s->mtl_cnt = 0;

  s->meshes = malloc(max_mesh_cnt * sizeof(*s->meshes));
  s->mesh_cnt = 0;

  s->instances = malloc(max_inst_cnt * sizeof(*s->instances));
  s->inst_cnt = 0;

  s->cams = malloc(max_cam_cnt * sizeof(*s->cams));
  s->cam_cnt = 0;

  s->bg_col = (vec3){0.0f, 0.0f, 0.0f};
}

void escene_release(escene *s)
{
  for(uint16_t i = 0; i < s->mesh_cnt; i++)
    emesh_release(&s->meshes[i]);

  free(s->cams);
  free(s->instances);
  free(s->meshes);
  free(s->mtls);
}

uint16_t escene_add_mtl(escene *s, mtl *mtl)
{
  memcpy(&s->mtls[s->mtl_cnt], mtl, sizeof(*s->mtls));
  return s->mtl_cnt++;
}

uint16_t escene_add_cam(escene *s, vec3 pos, vec3 dir, float vert_fov)
{
  s->cams[s->cam_cnt] = (ecam){.pos = pos, .dir = dir, .vert_fov = vert_fov};
  return s->cam_cnt++;
}

uint16_t escene_add_inst(escene *s, uint16_t mesh_id, uint16_t flags,
                         vec3 scale, float *rot, vec3 trans)
{
  s->instances[s->inst_cnt] = (einst){
      .mesh_id = (flags << 16) | mesh_id, .scale = scale, .trans = trans};
  memcpy(&s->instances[s->inst_cnt].rot, rot, 4 * sizeof(*rot));
  return s->inst_cnt++;
}

emesh *escene_acquire_mesh(escene *s)
{
  return &s->meshes[s->mesh_cnt];
}

uint16_t escene_attach_mesh(escene *s, emesh *m)
{
  return s->mesh_cnt++;
}

uint16_t calc_mtl_size(const mtl *m)
{
  return sizeof(m->col) + sizeof(m->metallic) + sizeof(m->roughness) +
         sizeof(m->ior) + sizeof(uint8_t); // Flags
}

uint32_t escene_calc_size(const escene *s)
{
  uint32_t sz = 0;

  sz += sizeof(s->mtl_cnt);
  sz += sizeof(s->cam_cnt);
  sz += sizeof(s->inst_cnt);
  sz += sizeof(s->mesh_cnt);
  sz += sizeof(s->bg_col);

  sz += s->mtl_cnt * calc_mtl_size(&s->mtls[0]);

  sz += s->cam_cnt * ecam_calc_size(&s->cams[0]);

  sz += s->inst_cnt * einst_calc_size(&s->instances[0]);

  for(uint16_t i = 0; i < s->mesh_cnt; i++)
    sz += emesh_calc_size(&s->meshes[i]);

  return sz;
}
