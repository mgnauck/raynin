#include "mesh.h"
#include "sutil.h"
#include "tri.h"

void mesh_init(mesh *m, uint32_t tri_cnt)
{
  m->tri_cnt  = tri_cnt;
  m->tris     = malloc(tri_cnt * sizeof(*m->tris));
  m->centers  = malloc(tri_cnt * sizeof(*m->centers));
  m->ofs      = 0;
}

void mesh_release(mesh *m)
{
  free(m->centers);
  free(m->tris);
}
