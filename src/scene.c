#include "scene.h"
#include "sutil.h"
#include "buf.h"
#include "mesh.h"
#include "inst.h"
#include "mat.h"
#include "bvh.h"
#include "tlas.h"

void scene_init(scene *s, uint32_t mesh_cnt, uint32_t inst_cnt, uint32_t mat_cnt)
{
  s->mesh_cnt = mesh_cnt;
  s->inst_cnt = inst_cnt;
  s->mat_cnt = mat_cnt;

  s->meshes = malloc(mesh_cnt * sizeof(mesh));
  s->instances = buf_acquire(INST, inst_cnt);
  s->materials = buf_acquire(MAT, mat_cnt);
  s->bvhs = malloc(mesh_cnt * sizeof(bvh));
  s->tlas_nodes = buf_acquire(TLAS_NODE, 2 * inst_cnt + 1);
}

void scene_release(scene *s)
{
  for(uint32_t i=0; i<s->mesh_cnt; i++)
    mesh_release(&s->meshes[i]);
  free(s->meshes);
  free(s->bvhs);
}
