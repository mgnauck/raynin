#include "emesh.h"

#include "../base/log.h"
#include "../base/walloc.h"
#include "../util/vec3.h"
#include "export.h"
#include "ieutil.h"

void emesh_init(emesh *m, uint32_t vertex_cnt, uint16_t index_cnt)
{
  m->vertices = malloc(vertex_cnt * sizeof(*m->vertices));
  m->normals = malloc(vertex_cnt * sizeof(*m->normals));
  m->indices = malloc(index_cnt * sizeof(*m->indices));
  m->vertex_cnt = 0;
  m->index_cnt = 0;
}

void emesh_release(emesh *m)
{
  free(m->indices);
  free(m->normals);
  free(m->vertices);
}

uint32_t emesh_calc_size(const emesh *m)
{
  uint32_t sz = 0;

  sz += sizeof(m->mtl_id);
  sz += sizeof(m->type);

  if(m->type == OT_MESH) {
#ifndef EXPORT_TRIS
    sz += sizeof(m->vertex_cnt);
    sz += sizeof(m->index_cnt);
    sz += sizeof(m->vertices_ofs);
#ifdef EXPORT_NORMALS
    sz += sizeof(m->normals_ofs);
#endif
    sz += sizeof(m->indices_ofs);
#ifdef EXPORT_NORMALS
    // Vertices, normals, indices
    sz += m->vertex_cnt * (sizeof(*m->vertices) + sizeof(*m->normals)) +
          m->index_cnt * sizeof(*m->indices);
#else
    // Vertices, indices
    sz += m->vertex_cnt * sizeof(*m->vertices) +
          m->index_cnt * sizeof(*m->indices);
#endif
#else
    // Vertices as tris
    sz += sizeof(m->index_cnt);
    sz += sizeof(m->vertices_ofs);
    sz += m->index_cnt * sizeof(*m->vertices);
#endif
  } else {
    sz += sizeof(m->subx);
    sz += sizeof(m->suby);
    sz += sizeof(m->in_radius);
  }

  return sz;
}

uint8_t *emesh_write_primitive(uint8_t *dst, const emesh *m)
{
  dst = ie_write(dst, &m->mtl_id, sizeof(m->mtl_id));
  dst = ie_write(dst, &m->type, sizeof(m->type)); // flags << 4 | type

  if(m->type == OT_MESH) {
#ifndef EXPORT_TRIS
    // Vertices get exported as vertice array (indexed)
    dst = ie_write(dst, &m->vertex_cnt, sizeof(m->vertex_cnt));     // u16
    dst = ie_write(dst, &m->index_cnt, sizeof(m->index_cnt));       // u16
    dst = ie_write(dst, &m->vertices_ofs, sizeof(m->vertices_ofs)); // u32
#ifdef EXPORT_NORMALS
    dst = ie_write(dst, &m->normals_ofs, sizeof(m->normals_ofs)); // u32
#endif
    dst = ie_write(dst, &m->indices_ofs, sizeof(m->indices_ofs)); // u32
    // logc("Wrote mesh with %i vcnt, %i vofs, %i nofs, %i iofs, %i icnt",
    // m->vertex_cnt, m->vertices_ofs, m->normals_ofs, m->indices_ofs,
    // m->index_cnt);
#else
    // Vertices get exported as tris (not indexed)
    dst = ie_write(dst, &m->index_cnt, sizeof(m->index_cnt));       // u16
    dst = ie_write(dst, &m->vertices_ofs, sizeof(m->vertices_ofs)); // u32
    // logc("Wrote mesh with %i vcnt, %i vofs", m->vertex_cnt, m->vertices_ofs);
#endif
  } else {
    dst = ie_write(dst, &m->subx, sizeof(m->subx));
    dst = ie_write(dst, &m->suby, sizeof(m->suby));
    dst = ie_write(dst, &m->in_radius, sizeof(m->in_radius));
  }

  return dst;
}
