#include "scene.h"
#include "sutil.h"
#include "buf.h"
#include "mesh.h"
#include "inst.h"
#include "mat.h"
#include "bvh.h"
#include "tlas.h"

void scene_init(scene *s, size_t mesh_cnt, size_t inst_cnt, size_t mat_cnt)
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
  free(s->meshes);
  free(s->bvhs);
}
