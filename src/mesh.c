#include "mesh.h"
#include "buf.h"

void mesh_init(mesh *m, uint32_t tri_cnt)
{
  m->tri_cnt = tri_cnt;

  m->tris = buf_acquire(TRI, tri_cnt);
  m->tris_data = buf_acquire(TRI_DATA, tri_cnt);
}
