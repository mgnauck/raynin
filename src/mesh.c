#include "mesh.h"
#include "sutil.h"
#include "mutil.h"
#include "buf.h"
#include "tri.h"

void mesh_init(mesh *m, uint32_t tri_cnt)
{
  m->tri_cnt = tri_cnt;
  
  m->tris = buf_acquire(TRI, tri_cnt);
  m->centers = malloc(tri_cnt * sizeof(*m->centers));
}

void mesh_read(mesh *m, const uint8_t *data)
{
  uint32_t ofs = 0;

  uint32_t vertex_cnt = *(uint32_t *)(data + ofs);
  ofs += sizeof(vertex_cnt);

  uint32_t normal_cnt = *(uint32_t *)(data + ofs);
  ofs += sizeof(normal_cnt);
  
  uint32_t uv_cnt = *(uint32_t *)(data + ofs); 
  ofs += sizeof(uv_cnt);

  m->tri_cnt = *(uint32_t *)(data + ofs);
  ofs += sizeof(m->tri_cnt);

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
    tri *t = &m->tris[i];
    uint32_t index = 3 * items * i;

    memcpy(&t->v0, &vertices[3 * indices[index]], sizeof(t->v0));
    memcpy(&t->v1, &vertices[3 * indices[index + items]], sizeof(t->v1));
    memcpy(&t->v2, &vertices[3 * indices[index + items + items]], sizeof(t->v2));

    if(uv_cnt > 0 ) {
      memcpy(t->uv0, &uvs[2 * indices[index + 1]], 2 * sizeof(*t->uv0));
      memcpy(t->uv1, &uvs[2 * indices[index + items + 1]], 2 * sizeof(*t->uv1));
      memcpy(t->uv2, &uvs[2 * indices[index + items + items + 1]], 2 * sizeof(*t->uv2));
    }
    
    if(normal_cnt > 0) {
      memcpy(&t->n0, &normals[3 * indices[index + 2]], sizeof(t->n0));
      memcpy(&t->n1, &normals[3 * indices[index + items + 2]], sizeof(t->n1));
      memcpy(&t->n2, &normals[3 * indices[index + items + items + 2]], sizeof(t->n2));
    }

    m->centers[i] = tri_calc_center(t);
  }
}

void mesh_make_quad(mesh *m, vec3 pos, vec3 nrm, float w, float h)
{
  mesh_init(m, 2);
  
  nrm = vec3_unit(nrm);
  
  vec3 r = (fabsf(nrm.x) > 0.9) ? (vec3){ 0.0, 1.0, 0.0 } : (vec3){ 0.0, 0.0, 1.0 };
  vec3 t = vec3_cross(nrm, r);
  vec3 b = vec3_cross(t, nrm);

  t = vec3_scale(t, 0.5f * w);
  b = vec3_scale(b, 0.5f * h);

  tri *tri = &m->tris[0];
  tri->v0 = vec3_sub(vec3_sub(pos, t), b);
  tri->v1 = vec3_add(vec3_sub(pos, t), b);
  tri->v2 = vec3_add(vec3_add(pos, t), b);
  tri->n0 = tri->n1 = tri->n2 = nrm;
  tri->uv0[0] = 0.0f; tri->uv0[1] = 0.0f;
  tri->uv1[0] = 0.0f; tri->uv1[1] = 1.0f;
  tri->uv2[0] = 1.0f; tri->uv2[1] = 1.0f;
  m->centers[0] = tri_calc_center(tri);

  tri = &m->tris[1];
  tri->v0 = vec3_sub(vec3_sub(pos, t), b);
  tri->v1 = vec3_add(vec3_add(pos, t), b);
  tri->v2 = vec3_sub(vec3_add(pos, t), b);
  tri->n0 = tri->n1 = tri->n2 = nrm;
  tri->uv0[0] = 0.0f; tri->uv0[1] = 0.0f;
  tri->uv1[0] = 1.0f; tri->uv1[1] = 1.0f;
  tri->uv2[0] = 1.0f; tri->uv2[1] = 0.0f;
  m->centers[1] = tri_calc_center(tri);
}

void mesh_make_icosahedron(mesh *m)
{
  float phi = 0.5f * (1.0f + sqrtf(5.0f));

  vec3 verts[12] = {
    { -1.0f,  phi, 0.0f },
    {  1.0f,  phi, 0.0f },
    { -1.0f, -phi, 0.0f },
    {  1.0f, -phi, 0.0f },
    
    { 0.0f, -1.0f,  phi },
    { 0.0f,  1.0f,  phi },
    { 0.0f, -1.0f, -phi },
    { 0.0f,  1.0f, -phi },
    
    {  phi, 0.0f, -1.0f },
    {  phi, 0.0f,  1.0f },
    { -phi, 0.0f, -1.0f },
    { -phi, 0.0f,  1.0f },
  };

  for(uint8_t i=0; i<12; i++)
    verts[i] = vec3_unit(verts[i]);

  mesh_init(m, 20);

  m->tris[ 0] = (tri){ .v0 = verts[ 0], .v1 = verts[11], .v2 = verts[ 5] };
  m->tris[ 1] = (tri){ .v0 = verts[ 0], .v1 = verts[ 5], .v2 = verts[ 1] };
  m->tris[ 2] = (tri){ .v0 = verts[ 0], .v1 = verts[ 1], .v2 = verts[ 7] };
  m->tris[ 3] = (tri){ .v0 = verts[ 0], .v1 = verts[ 7], .v2 = verts[10] };
  m->tris[ 4] = (tri){ .v0 = verts[ 0], .v1 = verts[10], .v2 = verts[11] };

  m->tris[ 5] = (tri){ .v0 = verts[ 1], .v1 = verts[ 5], .v2 = verts[ 9] };
  m->tris[ 6] = (tri){ .v0 = verts[ 5], .v1 = verts[11], .v2 = verts[ 4] };
  m->tris[ 7] = (tri){ .v0 = verts[11], .v1 = verts[10], .v2 = verts[ 2] };
  m->tris[ 8] = (tri){ .v0 = verts[10], .v1 = verts[ 7], .v2 = verts[ 6] };
  m->tris[ 9] = (tri){ .v0 = verts[ 7], .v1 = verts[ 1], .v2 = verts[ 8] };

  m->tris[10] = (tri){ .v0 = verts[ 3], .v1 = verts[ 9], .v2 = verts[ 4] };
  m->tris[11] = (tri){ .v0 = verts[ 3], .v1 = verts[ 4], .v2 = verts[ 2] };
  m->tris[12] = (tri){ .v0 = verts[ 3], .v1 = verts[ 2], .v2 = verts[ 6] };
  m->tris[13] = (tri){ .v0 = verts[ 3], .v1 = verts[ 6], .v2 = verts[ 8] };
  m->tris[14] = (tri){ .v0 = verts[ 3], .v1 = verts[ 8], .v2 = verts[ 9] };

  m->tris[15] = (tri){ .v0 = verts[ 4], .v1 = verts[ 9], .v2 = verts[ 5] };
  m->tris[16] = (tri){ .v0 = verts[ 2], .v1 = verts[ 4], .v2 = verts[11] };
  m->tris[17] = (tri){ .v0 = verts[ 6], .v1 = verts[ 2], .v2 = verts[10] };
  m->tris[18] = (tri){ .v0 = verts[ 8], .v1 = verts[ 6], .v2 = verts[ 7] };
  m->tris[19] = (tri){ .v0 = verts[ 9], .v1 = verts[ 8], .v2 = verts[ 1] };

  for(uint8_t i=0; i<20; i++) {
    tri *t = &m->tris[i];
    
    t->n0 = t->v0;
    t->n1 = t->v1;
    t->n2 = t->v2;
    
    // TODO uvs

    m->centers[i] = tri_calc_center(t);
  }
}

void mesh_release(mesh *m)
{
  free(m->centers);
}
