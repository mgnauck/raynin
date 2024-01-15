#include "mesh.h"
#include "sutil.h"
#include "tri.h"
#include "bvh.h"

mesh *mesh_create(vec3 *data, size_t data_size)
{
  mesh *m = malloc(sizeof(*m));
  
  m->tri_cnt = data_size / 3;
  m->tris = malloc(m->tri_cnt * sizeof(*m->tris));
  m->centers = malloc(m->tri_cnt * sizeof(*m->centers));

  // TEMP
  for(size_t i=0; i<m->tri_cnt; i++) {
    tri *tri = &m->tris[i];
    tri->v[0] = vec3_sub(vec3_scale(data[3 * i + 0], 9.0f), (vec3){ 5.0f, 5.0f, 5.0f });
    tri->v[1] = vec3_add(tri->v[0], data[3 * i + 1]);
    tri->v[2] = vec3_add(tri->v[0], data[3 * i + 2]);
    m->centers[i] = tri_calc_center(tri);
  }

  m->bvh = bvh_init(m);
  bvh_create(m->bvh);

  return m;
}

void mesh_release(mesh *m)
{
  bvh_release(m->bvh);
  
  free(m->tris);
  free(m->centers);
  free(m);
}
