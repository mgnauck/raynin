#include "ex_mesh.h"
#include "../sys/sutil.h"
#include "../util/vec3.h"
#include "ieutil.h"

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

uint32_t ex_mesh_calc_mesh_data_size(ex_mesh const *m)
{
  return m->vertex_cnt * (sizeof(*m->vertices) + sizeof(*m->normals)) +
    m->index_cnt * sizeof(*m->indices);
}

uint32_t ex_mesh_calc_size(ex_mesh const *m)
{
  uint32_t sz = 0;

  sz += sizeof(m->mtl_id);
  sz += sizeof(m->type);
  sz += sizeof(m->subx);
  sz += sizeof(m->suby);
  sz += sizeof(m->flags);
  sz += sizeof(m->in_radius);

  if(m->type == OT_MESH) {
    sz += sizeof(m->vertex_cnt);
    sz += sizeof(m->index_cnt);
    sz += sizeof(m->vertices_ofs);
    sz += sizeof(m->normals_ofs);
    sz += sizeof(m->indices_ofs);
    sz += ex_mesh_calc_mesh_data_size(m);
  }

  return sz;
}

uint8_t *ex_mesh_write_primitive(ex_mesh const *m, uint8_t *dst)
{
  dst = ie_write(dst, &m->mtl_id, sizeof(m->mtl_id));
  dst = ie_write(dst, &m->type, sizeof(m->type));
  dst = ie_write(dst, &m->subx, sizeof(m->subx));
  dst = ie_write(dst, &m->suby, sizeof(m->suby));
  dst = ie_write(dst, &m->flags, sizeof(m->flags));
  dst = ie_write(dst, &m->in_radius, sizeof(m->in_radius));

  if(m->type == OT_MESH) {
    dst = ie_write(dst, &m->vertex_cnt, sizeof(m->vertex_cnt));
    dst = ie_write(dst, &m->index_cnt, sizeof(m->index_cnt));
    dst = ie_write(dst, &m->vertices_ofs, sizeof(m->vertices_ofs));
    dst = ie_write(dst, &m->normals_ofs, sizeof(m->normals_ofs));
    dst = ie_write(dst, &m->indices_ofs, sizeof(m->indices_ofs));
  }

  return dst;
}

uint8_t *ex_mesh_write_vertices(ex_mesh const *m, uint8_t *dst)
{
  return ie_write(dst, m->vertices, m->vertex_cnt * sizeof(*m->vertices));
}

uint8_t *ex_mesh_write_normals(ex_mesh const *m, uint8_t *dst)
{
  return ie_write(dst, m->normals, m->vertex_cnt * sizeof(*m->normals));
}

uint8_t *ex_mesh_write_indices(ex_mesh const *m, uint8_t *dst)
{
  return ie_write(dst, m->indices, m->index_cnt * sizeof(*m->indices));
}
