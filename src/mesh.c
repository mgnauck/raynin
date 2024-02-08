#include "mesh.h"
#include "sutil.h"
#include "buf.h"
#include "tri.h"

void mesh_init(mesh *m, uint32_t tri_cnt)
{
  m->tri_cnt = tri_cnt;

  m->tris = buf_acquire(TRI, tri_cnt);
  m->tris_data = buf_acquire(TRI_DATA, tri_cnt);
}

void mesh_read_bin(mesh *m, const uint8_t *data)
{
  uint32_t ofs = 0;

  uint32_t vertex_cnt = *(uint32_t *)(data + ofs);
  ofs += sizeof(vertex_cnt);

  uint32_t normal_cnt = *(uint32_t *)(data + ofs);
  ofs += sizeof(vertex_cnt);
  
  uint32_t uv_cnt = *(uint32_t *)(data + ofs); 
  ofs += sizeof(vertex_cnt);

  m->tri_cnt = *(uint32_t *)(data + ofs);
  ofs += sizeof(vertex_cnt);

  float *vertices = (float *)(data + ofs);
  ofs += vertex_cnt * 3 * sizeof(*vertices);

  float *normals = (float *)(data + ofs);
  ofs += normal_cnt * 3 * sizeof(*normals);

  float *uvs = (float *)(data + ofs);
  ofs += uv_cnt * 2 * sizeof(*uvs);

  uint32_t *indices = (uint32_t *)(data + ofs);

  mesh_init(m, m->tri_cnt);

  uint32_t items = (1 + (uv_cnt > 0 ? 1 : 0) + (normal_cnt > 0 ? 1 : 0));

  for(uint32_t i=0; i<m->tri_cnt; i++) {
    uint32_t index = 3 * items * i;
 
    tri *t = &m->tris[i];
    memcpy(&t->v0, &vertices[3 * indices[index]], sizeof(t->v0));
    memcpy(&t->v1, &vertices[3 * indices[index + items]], sizeof(t->v1));
    memcpy(&t->v2, &vertices[3 * indices[index + items + items]], sizeof(t->v2));

    tri_data *td = &m->tris_data[i];
    if(uv_cnt > 0 ) {
      memcpy(td->uv0, &uvs[2 * indices[index + 1]], 2 * sizeof(*td->uv0));
      memcpy(td->uv1, &uvs[2 * indices[index + items + 1]], 2 * sizeof(*td->uv1));
      memcpy(td->uv2, &uvs[2 * indices[index + items + items + 1]], 2 * sizeof(*td->uv2));
    }
    if(normal_cnt > 0) {
      memcpy(&td->n0, &normals[3 * indices[index + 2]], sizeof(td->n0));
      memcpy(&td->n1, &normals[3 * indices[index + items + 2]], sizeof(td->n1));
      memcpy(&td->n2, &normals[3 * indices[index + items + items + 2]], sizeof(td->n2));
    }

    tri_calc_center(t);
  }
}
