#include "mesh.h"
#include "sutil.h"
#include "mutil.h"
#include "buf.h"
#include "tri.h"
#include "cfg.h" // TEXTURE_SUPPORT flag

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
#ifdef TEXTURE_SUPPORT // Untested
    if(uv_cnt > 0 ) {
      memcpy(t->uv0, &uvs[2 * indices[index + 1]], 2 * sizeof(*t->uv0));
      memcpy(t->uv1, &uvs[2 * indices[index + items + 1]], 2 * sizeof(*t->uv1));
      memcpy(t->uv2, &uvs[2 * indices[index + items + items + 1]], 2 * sizeof(*t->uv2));
    }
#endif
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
#ifdef TEXTURE_SUPPORT // Untested
  tri->uv0[0] = 0.0f; tri->uv0[1] = 0.0f;
  tri->uv1[0] = 0.0f; tri->uv1[1] = 1.0f;
  tri->uv2[0] = 1.0f; tri->uv2[1] = 1.0f;
#endif
  m->centers[0] = tri_calc_center(tri);

  tri = &m->tris[1];
  tri->v0 = vec3_sub(vec3_sub(pos, t), b);
  tri->v1 = vec3_add(vec3_add(pos, t), b);
  tri->v2 = vec3_sub(vec3_add(pos, t), b);
  tri->n0 = tri->n1 = tri->n2 = nrm;
#ifdef TEXTURE_SUPPORT // Untested
  tri->uv0[0] = 0.0f; tri->uv0[1] = 0.0f;
  tri->uv1[0] = 1.0f; tri->uv1[1] = 1.0f;
  tri->uv2[0] = 1.0f; tri->uv2[1] = 0.0f;
#endif
  m->centers[1] = tri_calc_center(tri);
}

void mesh_make_icosphere(mesh *m, uint8_t steps, bool face_normals)
{
  mesh_init(m, 20 * (1 << (2 * steps)));

  float phi = 0.5f * (1.0f + sqrtf(5.0f));
  
  m->tris[ 0] = (tri){ .v0 = (vec3){ -1.0, phi, 0.0 }, .v1 = (vec3){ 0.0, 1.0, phi }, .v2 = (vec3){ 1.0, phi, 0.0 } };
  m->tris[ 1] = (tri){ .v0 = (vec3){ 1.0, phi, 0.0 }, .v1 = (vec3){ 0.0, 1.0, -phi }, .v2 = (vec3){ -1.0, phi, 0.0 } };
  m->tris[ 2] = (tri){ .v0 = (vec3){ 1.0, phi, 0.0 }, .v1 = (vec3){ 0.0, 1.0, phi }, .v2 = (vec3){ phi, 0.0, 1.0 } };
  m->tris[ 3] = (tri){ .v0 = (vec3){ 1.0, phi, 0.0 }, .v1 = (vec3){ phi, 0.0, -1.0 }, .v2 = (vec3){ 0.0, 1.0, -phi } };
  m->tris[ 4] = (tri){ .v0 = (vec3){ phi, 0.0, -1.0 }, .v1 = (vec3){ 1.0, phi, 0.0 }, .v2 = (vec3){ phi, 0.0, 1.0 } };
  m->tris[ 5] = (tri){ .v0 = (vec3){ -1.0, -phi, 0.0 }, .v1 = (vec3){ 1.0, -phi, 0.0 }, .v2 = (vec3){ 0.0, -1.0, phi } };
  m->tris[ 6] = (tri){ .v0 = (vec3){ -1.0, -phi, 0.0 }, .v1 = (vec3){ 0.0, -1.0, -phi }, .v2 = (vec3){ 1.0, -phi, 0.0 } };
  m->tris[ 7] = (tri){ .v0 = (vec3){ -1.0, -phi, 0.0 }, .v1 = (vec3){ 0.0, -1.0, phi }, .v2 = (vec3){ -phi, 0.0, 1.0 } };
  m->tris[ 8] = (tri){ .v0 = (vec3){ -1.0, -phi, 0.0 }, .v1 = (vec3){ -phi, 0.0, -1.0 }, .v2 = (vec3){ 0.0, -1.0, -phi } };
  m->tris[ 9] = (tri){ .v0 = (vec3){ -phi, 0.0, 1.0 }, .v1 = (vec3){ -phi, 0.0, -1.0 }, .v2 = (vec3){ -1.0, -phi, 0.0 } };
  m->tris[10] = (tri){ .v0 = (vec3){ -1.0, phi, 0.0 }, .v1 = (vec3){ -phi, 0.0, 1.0 }, .v2 = (vec3){ 0.0, 1.0, phi } };
  m->tris[11] = (tri){ .v0 = (vec3){ -1.0, phi, 0.0 }, .v1 = (vec3){ 0.0, 1.0, -phi }, .v2 = (vec3){ -phi, 0.0, -1.0 } };
  m->tris[12] = (tri){ .v0 = (vec3){ -1.0, phi, 0.0 }, .v1 = (vec3){ -phi, 0.0, -1.0 }, .v2 = (vec3){ -phi, 0.0, 1.0 } };
  m->tris[13] = (tri){ .v0 = (vec3){ 1.0, -phi, 0.0 }, .v1 = (vec3){ phi, 0.0, 1.0 }, .v2 = (vec3){ 0.0, -1.0, phi } };
  m->tris[14] = (tri){ .v0 = (vec3){ 1.0, -phi, 0.0 }, .v1 = (vec3){ 0.0, -1.0, -phi }, .v2 = (vec3){ phi, 0.0, -1.0 } };
  m->tris[15] = (tri){ .v0 = (vec3){ 1.0, -phi, 0.0 }, .v1 = (vec3){ phi, 0.0, -1.0 }, .v2 = (vec3){ phi, 0.0, 1.0 } };
  m->tris[16] = (tri){ .v0 = (vec3){ 0.0, -1.0, -phi }, .v1 = (vec3){ -phi, 0.0, -1.0 }, .v2 = (vec3){ 0.0, 1.0, -phi } };
  m->tris[17] = (tri){ .v0 = (vec3){ 0.0, -1.0, -phi }, .v1 = (vec3){ 0.0, 1.0, -phi }, .v2 = (vec3){ phi, 0.0, -1.0 } };
  m->tris[18] = (tri){ .v0 = (vec3){ 0.0, 1.0, phi }, .v1 = (vec3){ -phi, 0.0, 1.0 }, .v2 = (vec3){ 0.0, -1.0, phi } };
  m->tris[19] = (tri){ .v0 = (vec3){ 0.0, 1.0, phi }, .v1 = (vec3){ 0.0, -1.0, phi }, .v2 = (vec3){ phi, 0.0, 1.0 } };

  for(uint8_t i=0; i<20; i++) {
    tri *t = &m->tris[i];
    t->v0 = vec3_unit(t->v0);
    t->v1 = vec3_unit(t->v1);
    t->v2 = vec3_unit(t->v2);
  }

  // Alloc temp tri buffer for last but one subdiv level
  tri *temp_tris = (steps > 0) ? malloc(20 * (1 << (2 * (steps - 1))) * sizeof(*temp_tris)) : NULL;

  // Subdivide
  for(uint8_t j=0; j<steps; j++)
  {
    uint32_t curr_tri_cnt = 20 * (1 << (2 * j)); 
    
    memcpy(temp_tris, m->tris, curr_tri_cnt * sizeof(*temp_tris));

    for(uint32_t i=0; i<curr_tri_cnt; i++) {
      tri *t = &temp_tris[i];
      vec3 a = vec3_unit(vec3_scale(vec3_add(t->v0, t->v1), 0.5f));
      vec3 b = vec3_unit(vec3_scale(vec3_add(t->v1, t->v2), 0.5f));
      vec3 c = vec3_unit(vec3_scale(vec3_add(t->v2, t->v0), 0.5f));

      uint32_t ofs = 4 * i;
      m->tris[ofs + 0] = (tri){ .v0 = t->v0, .v1 =     a, .v2 =     c };
      m->tris[ofs + 1] = (tri){ .v0 =     a, .v1 = t->v1, .v2 =     b };
      m->tris[ofs + 2] = (tri){ .v0 =     c, .v1 =     b, .v2 = t->v2 };
      m->tris[ofs + 3] = (tri){ .v0 =     a, .v1 =     b, .v2 =     c };
    }
  }

  free(temp_tris);

  // Centers, normals and uvs
  for(uint32_t i=0; i<m->tri_cnt; i++) {
    tri *t = &m->tris[i];
    m->centers[i] = tri_calc_center(t);
    if(face_normals) {
      vec3 n = vec3_unit(m->centers[i]);
      t->n0 = n;
      t->n1 = n;
      t->n2 = n;
    } else {
      t->n0 = t->v0;
      t->n1 = t->v1;
      t->n2 = t->v2;
    }
#ifdef TEXTURE_SUPPORT // Untested
    t->uv0[0] = asinf(t->v0.x) / PI + 0.5f;
    t->uv0[1] = asinf(t->v0.y) / PI + 0.5f;
    t->uv1[0] = asinf(t->v1.x) / PI + 0.5f;
    t->uv1[1] = asinf(t->v1.y) / PI + 0.5f;
    t->uv2[0] = asinf(t->v2.x) / PI + 0.5f;
    t->uv2[1] = asinf(t->v2.y) / PI + 0.5f;
#endif
  }
}

void mesh_make_uvsphere(mesh *m, float radius, uint32_t subx, uint32_t suby, bool face_normals)
{
  mesh_init(m, 2 * subx * suby);

  float dphi = 2 * PI / subx;
  float dtheta = PI / suby;
  float inv_r = 1.0f / radius;

  float theta = 0.0f;
  for(uint32_t j=0; j<suby; j++) {
    float phi = 0.0f;
    for(uint32_t i=0; i<subx; i++) {
      uint32_t ofs = 2 * (subx * j + i);

      vec3 a = vec3_scale(vec3_spherical(theta + dtheta, phi), radius);
      vec3 b = vec3_scale(vec3_spherical(theta, phi), radius);
      vec3 c = vec3_scale(vec3_spherical(theta, phi + dphi), radius);
      vec3 d = vec3_scale(vec3_spherical(theta + dtheta, phi + dphi), radius);

      tri *t1 = &m->tris[ofs];
      *t1 = (tri){ .v0 = a, .v1 = b, .v2 = c,
#ifdef TEXTURE_SUPPORT // Untested
        .uv0[0] = phi / 2.0f * PI, .uv0[1] = (theta + dtheta) / PI,
        .uv1[0] = phi / 2.0f * PI, .uv1[1] = theta / PI,
        .uv2[0] = (phi + dphi) / 2.0f * PI, .uv2[1] = theta / PI,
#endif
      };
      
      m->centers[ofs] = tri_calc_center(t1);
      
      if(face_normals) {
        t1->n0 = vec3_unit(vec3_cross(vec3_sub(a, b), vec3_sub(c, b)));
        t1->n1 = t1->n0;
        t1->n2 = t1->n0;
      } else {
        t1->n0 = vec3_scale(a, inv_r);
        t1->n1 = vec3_scale(b, inv_r);
        t1->n2 = vec3_scale(c, inv_r);
      }

      tri *t2 = &m->tris[ofs + 1];
      *t2 = (tri){ .v0 = a, .v1 = c, .v2 = d,
#ifdef TEXTURE_SUPPORT // Untested
        .uv0[0] = t1->uv0[0], .uv0[1] = t1->uv0[1],
        .uv1[0] = t1->uv2[0], .uv1[1] = t1->uv1[1],
        .uv2[0] = t1->uv2[0], .uv2[1] = t1->uv0[1],
#endif
      };
      
      m->centers[ofs + 1] = tri_calc_center(t2);
      
      if(face_normals) {
        t2->n0 = vec3_unit(vec3_cross(vec3_sub(a, c), vec3_sub(d, c)));
        t2->n1 = t2->n0;
        t2->n2 = t2->n0;
      } else {
        t2->n0 = vec3_scale(a, inv_r);
        t2->n1 = vec3_scale(c, inv_r);
        t2->n2 = vec3_scale(d, inv_r);
      }

      phi += dphi;
    }

    theta += dtheta;
  }
}

void mesh_make_uvcylinder(mesh *m, float radius, float height, uint32_t subx, uint32_t suby, bool face_normals)
{
  mesh_init(m, 2 * subx * suby);

  float dphi = 2 * PI / subx;
  float dh = height / suby;
  float inv_r = 1.0f / radius;

  float h = 0.0f;
  for(uint32_t j=0; j<suby; j++) {
    float phi = 0.0f;
    for(uint32_t i=0; i<subx; i++) {
      uint32_t ofs = 2 * (subx * j + i);

      float x1 = radius * sinf(phi);
      float x2 = radius * sinf(phi + dphi);
      float y1 = -height * 0.5f + h;
      float y2 = -height * 0.5f + h + dh;
      float z1 = radius * cosf(phi);
      float z2 = radius * cosf(phi + dphi);

      vec3 a = { x1, y2, z1 };
      vec3 b = { x1, y1, z1 };
      vec3 c = { x2, y1, z2 };
      vec3 d = { x2, y2, z2 };

      tri *t1 = &m->tris[ofs];
      *t1 = (tri){ .v0 = a, .v1 = b, .v2 = c,
#ifdef TEXTURE_SUPPORT // Untested
        .uv0[0] = phi / 2.0f * PI, .uv0[1] = (h + dh) / height,
        .uv1[0] = phi / 2.0f * PI, .uv1[1] = h / height,
        .uv2[0] = (phi + dphi) / 2.0f * PI, .uv2[1] = h / height,
#endif
      };
      
      m->centers[ofs] = tri_calc_center(t1);
      
      if(face_normals) {
        t1->n0 = vec3_unit(vec3_cross(vec3_sub(a, b), vec3_sub(c, b)));
        t1->n1 = t1->n0;
        t1->n2 = t1->n0;
      } else {
        t1->n0 = vec3_scale((vec3){ x1, 0.0f, z1 }, inv_r);
        t1->n1 = t1->n0;
        t1->n2 = vec3_scale((vec3){ x2, 0.0f, z2 }, inv_r);
      }

      tri *t2 = &m->tris[ofs + 1];
      *t2 = (tri){ .v0 = a, .v1 = c, .v2 = d,
#ifdef TEXTURE_SUPPORT // Untested
        .uv0[0] = t1->uv0[0], .uv0[1] = t1->uv0[1],
        .uv1[0] = t1->uv2[0], .uv1[1] = t1->uv1[1],
        .uv2[0] = t1->uv2[0], .uv2[1] = t1->uv0[1],
#endif
      };
      
      m->centers[ofs + 1] = tri_calc_center(t2);
      
      if(face_normals) {
        t2->n0 = vec3_unit(vec3_cross(vec3_sub(a, c), vec3_sub(d, c)));
        t2->n1 = t2->n0;
        t2->n2 = t2->n0;
      } else {
        t2->n0 = vec3_scale((vec3){ x1, 0.0f, z1 }, inv_r);
        t2->n1 = vec3_scale((vec3){ x2, 0.0f, z2 }, inv_r);
        t2->n2 = t2->n1;
      }

      phi += dphi;
    }

    h += dh;
  }
}

void mesh_release(mesh *m)
{
  free(m->centers);
}
