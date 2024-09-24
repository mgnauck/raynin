#include "ex_mesh.h"
#include "../sys/sutil.h"
#include "../util/vec3.h"

void ex_mesh_init(ex_mesh *m, uint32_t vertex_cnt, uint16_t index_cnt)
{
  m->vertices = malloc(vertex_cnt * sizeof(*m->vertices));
  m->normals = malloc(vertex_cnt * sizeof(*m->normals));
  m->indices = malloc(index_cnt * sizeof(*m->indices));
}

void ex_mesh_release(ex_mesh *m)
{
  free(m->indices);
  free(m->normals);
  free(m->vertices);
}
