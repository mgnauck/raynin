#include "ex_scene.h"
#include "../scene/mtl.h"
#include "../sys/log.h"
#include "../sys/sutil.h"
#include "ex_cam.h"
#include "ex_inst.h"
#include "ex_mesh.h"

void ex_scene_init(ex_scene *s, uint16_t max_mesh_cnt, uint16_t max_mtl_cnt, uint16_t max_cam_cnt, uint16_t max_inst_cnt)
{
  s->mtls         = malloc(max_mtl_cnt * sizeof(*s->mtls));
  s->mtl_cnt      = 0;

  s->meshes       = malloc(max_mesh_cnt * sizeof(*s->meshes));
  s->mesh_cnt     = 0;

  s->instances    = malloc(max_inst_cnt * sizeof(*s->instances));
  s->inst_cnt     = 0;

  s->cams         = malloc(max_cam_cnt * sizeof(*s->cams));
  s->cam_cnt      = 0;

  s->bg_col       = (vec3){ 0.0f, 0.0f, 0.0f };
}

void ex_scene_release(ex_scene *s)
{
  for(uint16_t i=0; i<s->mesh_cnt; i++)
    ex_mesh_release(&s->meshes[i]);

  free(s->cams);
  free(s->instances);
  free(s->meshes);
  free(s->mtls);
}

uint16_t ex_scene_add_mtl(ex_scene *s, mtl *mtl)
{
  memcpy(&s->mtls[s->mtl_cnt], mtl, sizeof(*s->mtls));
  return s->mtl_cnt++;
}

uint16_t ex_scene_add_cam(ex_scene *s, vec3 eye, vec3 tgt, float vert_fov)
{
  s->cams[s->cam_cnt] = (ex_cam){ .eye = eye, .tgt = tgt, .vert_fov = vert_fov };
  return s->cam_cnt++;
}

uint16_t ex_scene_add_inst(ex_scene *s, uint16_t mesh_id, vec3 scale, float *rot, vec3 trans)
{
  s->instances[s->inst_cnt] = (ex_inst){ .mesh_id = mesh_id, .scale = scale, .trans = trans };
  memcpy(&s->instances[s->inst_cnt].rot, rot, 4 * sizeof(*rot));
  return s->inst_cnt++;
}

ex_mesh *ex_scene_acquire_mesh(ex_scene *s)
{
  return &s->meshes[s->mesh_cnt];
}

uint16_t ex_scene_attach_mesh(ex_scene *s, ex_mesh *m)
{
  return s->mesh_cnt++;
}

uint16_t calc_mtl_size(mtl const* m)
{
  return sizeof(m->col) + sizeof(m->metallic) + sizeof(m->roughness) +
    sizeof(m->ior) + sizeof(uint8_t); // Flags refractive << 1 | emissive
}

uint32_t ex_scene_calc_size(ex_scene const *s)
{
  uint32_t sz = 0;

  sz += sizeof(s->mtl_cnt);
  sz += sizeof(s->cam_cnt);
  sz += sizeof(s->inst_cnt);
  sz += sizeof(s->mesh_cnt);
  sz += sizeof(s->bg_col);

  sz += s->mtl_cnt * calc_mtl_size(&s->mtls[0]);
  logc("mtls: %i, size per: %i", s->mtl_cnt, calc_mtl_size(&s->mtls[0]));
  
  sz += s->cam_cnt * ex_cam_calc_size(&s->cams[0]);
  logc("cams: %i, size per: %i", s->cam_cnt, ex_cam_calc_size(&s->cams[0]));
  
  sz += s->inst_cnt * ex_inst_calc_size(&s->instances[0]);
  logc("insts: %i, size per: %i", s->inst_cnt, ex_inst_calc_size(&s->instances[0]));

  for(uint16_t i=0; i<s->mesh_cnt; i++) {
    sz += ex_mesh_calc_size(&s->meshes[i]);
    logc("mesh size: %i", ex_mesh_calc_size(&s->meshes[i]));
    if(s->meshes[i].type == OT_MESH) {
      logc("mesh vertex cnt: %i, index cnt: %i", s->meshes[i].vertex_cnt, s->meshes[i].index_cnt);
    }
  }

  logc("Total scene size: %i", sz);

  return sz;
}
