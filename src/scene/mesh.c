#include "mesh.h"
#include "../sys/mutil.h"
#include "../sys/sutil.h"
#include "tri.h"

void mesh_init(mesh *m, uint32_t tri_cnt)
{
  m->tri_cnt      = tri_cnt;
  m->tris         = malloc(tri_cnt * sizeof(*m->tris));
  m->centers      = malloc(tri_cnt * sizeof(*m->centers));
  m->is_emissive  = false;
  m->ofs          = 0;
}

void mesh_release(mesh *m)
{
  free(m->centers);
  free(m->tris);
}

void mesh_create_quad(mesh *m, uint32_t subx, uint32_t suby, uint32_t mtl, bool invert_normals)
{
  // Generate quad in XZ plane at origin with normal pointing up in Y
  mesh_init(m, 2 * subx * suby);

  float P = QUAD_DEFAULT_SIZE;
  float P2 = 0.5f * P;

  float dx = P / (float)subx;
  float dz = P / (float)suby;

  float inv = invert_normals ? -1.0f : 1.0f;

  uint32_t ofs = 0;

  float z = 0.0f;
  for(uint32_t j=0; j<suby; j++) {
    float x = 0.0f;
    for(uint32_t i=0; i<subx; i++) {
      // Tri 0
      tri *t = &m->tris[ofs];
      *t = (tri){
        .v0 = (vec3){ -P2 + x, 0.0, -P2 + z },
        .v1 = (vec3){ -P2 + x, 0.0, -P2 + z + dz },
        .v2 = (vec3){ -P2 + x + dx, 0.0, -P2 + z + dz },
        .mtl = mtl,
        .face_nrm = 1.0f
      };
      t->n0 = t->n1 = t->n2 = (vec3){ 0.0f, inv * 1.0f, 0.0f };
      m->centers[ofs++] = tri_calc_center(t);

      // Tri 1
      t = &m->tris[ofs];
      *t = (tri){
        .v0 = (vec3){ -P2 + x, 0.0, -P2 + z },
        .v1 = (vec3){ -P2 + x + dx, 0.0, -P2 + z + dz },
        .v2 = (vec3){ -P2 + x + dx, 0.0, -P2 + z },
        .mtl = mtl,
        .face_nrm = 1.0f
      };
      t->n0 = t->n1 = t->n2 = (vec3){ 0.0f, inv * 1.0f, 0.0f };
      m->centers[ofs++] = tri_calc_center(t);

      x += dx;
    }

    z += dz;
  }
}

void mesh_create_box(mesh *m, uint32_t mtl, bool invert_normals)
{
  mesh_init(m, 12);

  float inv = invert_normals ? -1.0f : 1.0f;

  m->tris[ 0] = (tri){ // front
    .v0 = (vec3){ -1.0f, 1.0f, 1.0f }, .v1 = (vec3){ -1.0f, -1.0f, 1.0f }, .v2 = (vec3){ 1.0f, -1.0f, 1.0f },
    .n0 = (vec3){ 0.0f, 0.0, inv * 1.0f }, .n1 = (vec3){ 0.0f, 0.0f, inv * 1.0f }, .n2 = (vec3){ 0.0f, 0.0f, inv * 1.0f },
  };
  m->tris[ 1] = (tri){ // front
    .v0 = (vec3){ -1.0f, 1.0f, 1.0f }, .v1 = (vec3){ 1.0f, -1.0f, 1.0f }, .v2 = (vec3){ 1.0f, 1.0f, 1.0f },
    .n0 = (vec3){ 0.0f, 0.0, inv * 1.0f }, .n1 = (vec3){ 0.0f, 0.0f, inv * 1.0f }, .n2 = (vec3){ 0.0f, 0.0f, inv * 1.0f },
  };
  m->tris[ 2] = (tri){ // back
    .v0 = (vec3){ 1.0f, 1.0f, -1.0f }, .v1 = (vec3){ 1.0f, -1.0f, -1.0f }, .v2 = (vec3){ -1.0f, -1.0f, -1.0f },
    .n0 = (vec3){ 0.0f, 0.0, inv * -1.0f }, .n1 = (vec3){ 0.0f, 0.0f, inv * -1.0f }, .n2 = (vec3){ 0.0f, 0.0f, inv * -1.0f },
  };
  m->tris[ 3] = (tri){ // back
    .v0 = (vec3){ 1.0f, 1.0f, -1.0f }, .v1 = (vec3){ -1.0f, -1.0f, -1.0f }, .v2 = (vec3){ -1.0f, 1.0f, -1.0f },
    .n0 = (vec3){ 0.0f, 0.0, inv * -1.0f }, .n1 = (vec3){ 0.0f, 0.0f, inv * -1.0f }, .n2 = (vec3){ 0.0f, 0.0f, inv * -1.0f },
  };
  m->tris[ 4] = (tri){ // left
    .v0 = (vec3){ -1.0f, 1.0f, -1.0f }, .v1 = (vec3){ -1.0f, -1.0f, -1.0f }, .v2 = (vec3){ -1.0f, -1.0f, 1.0f },
    .n0 = (vec3){ inv * -1.0f, 0.0, 0.0f }, .n1 = (vec3){ inv * -1.0f, 0.0f, 0.0f }, .n2 = (vec3){ inv * -1.0f, 0.0f, 0.0f },
  };
  m->tris[ 5] = (tri){ // left
    .v0 = (vec3){ -1.0f, 1.0f, -1.0f }, .v1 = (vec3){ -1.0f, -1.0f, 1.0f }, .v2 = (vec3){ -1.0f, 1.0f, 1.0f },
    .n0 = (vec3){ inv * -1.0f, 0.0, 0.0f }, .n1 = (vec3){ inv * -1.0f, 0.0f, 0.0f }, .n2 = (vec3){ inv * -1.0f, 0.0f, 0.0f },
  };
  m->tris[ 6] = (tri){ // right
    .v0 = (vec3){ 1.0f, 1.0f, 1.0f }, .v1 = (vec3){ 1.0f, -1.0f, 1.0f }, .v2 = (vec3){ 1.0f, -1.0f, -1.0f },
    .n0 = (vec3){ inv * 1.0f, 0.0, 0.0f }, .n1 = (vec3){ inv * 1.0f, 0.0f, 0.0f }, .n2 = (vec3){ inv * 1.0f, 0.0f, 0.0f },
  };
  m->tris[ 7] = (tri){ // right
    .v0 = (vec3){ 1.0f, 1.0f, 1.0f }, .v1 = (vec3){ 1.0f, -1.0f, -1.0f }, .v2 = (vec3){ 1.0f, 1.0f, -1.0f },
    .n0 = (vec3){ inv * 1.0f, 0.0, 0.0f }, .n1 = (vec3){ inv * 1.0f, 0.0f, 0.0f }, .n2 = (vec3){ inv * 1.0f, 0.0f, 0.0f },
  };
  m->tris[ 8] = (tri){ // top
    .v0 = (vec3){ -1.0f, 1.0f, -1.0f }, .v1 = (vec3){ -1.0f, 1.0f, 1.0f }, .v2 = (vec3){ 1.0f, 1.0f, 1.0f },
    .n0 = (vec3){ 0.0f, inv * 1.0, 0.0f }, .n1 = (vec3){ 0.0f, inv * 1.0f, 0.0f }, .n2 = (vec3){ 0.0f, inv * 1.0f, 0.0f },
  };
  m->tris[ 9] = (tri){ // top
    .v0 = (vec3){ -1.0f, 1.0f, -1.0f }, .v1 = (vec3){ 1.0f, 1.0f, 1.0f }, .v2 = (vec3){ 1.0f, 1.0f, -1.0f },
    .n0 = (vec3){ 0.0f, inv * 1.0, 0.0f }, .n1 = (vec3){ 0.0f, inv * 1.0f, 0.0f }, .n2 = (vec3){ 0.0f, inv * 1.0f, 0.0f },
  };
  m->tris[10] = (tri){ // bottom
    .v0 = (vec3){ 1.0f, -1.0f, -1.0f }, .v1 = (vec3){ 1.0f, -1.0f, 1.0f }, .v2 = (vec3){ -1.0f, -1.0f, 1.0f },
    .n0 = (vec3){ 0.0f, inv * -1.0, 0.0f }, .n1 = (vec3){ 0.0f, inv * -1.0f, 0.0f }, .n2 = (vec3){ 0.0f, inv * -1.0f, 0.0f },
  };
  m->tris[11] = (tri){ // bottom
    .v0 = (vec3){ 1.0f, -1.0f, -1.0f }, .v1 = (vec3){ -1.0f, -1.0f, 1.0f }, .v2 = (vec3){ -1.0f, -1.0f, -1.0f },
    .n0 = (vec3){ 0.0f, inv * -1.0, 0.0f }, .n1 = (vec3){ 0.0f, inv * -1.0f, 0.0f }, .n2 = (vec3){ 0.0f, inv * -1.0f, 0.0f },
  };

  for(uint32_t i=0; i<m->tri_cnt; i++) {
    tri *t = &m->tris[i];
    m->centers[i] = tri_calc_center(t);
    t->mtl = mtl;
    t->face_nrm = 1.0f;
  }
}

void mesh_create_uvsphere(mesh *m, float radius, uint32_t subx, uint32_t suby, uint32_t mtl, bool face_normals, bool invert_normals)
{
  mesh_init(m, 2 * subx * suby);

  float dphi = 2 * PI / subx;
  float dtheta = PI / suby;

  float inv_r = 1.0f / radius;

  float inv = invert_normals ? -1.0f : 1.0f;

  uint32_t ofs = 0;

  float theta = 0.0f;
  for(uint32_t j=0; j<suby; j++) {
    float phi = 0.0f;
    for(uint32_t i=0; i<subx; i++) {
      vec3 a = vec3_scale(vec3_spherical(theta + dtheta, phi), radius);
      vec3 b = vec3_scale(vec3_spherical(theta, phi), radius);
      vec3 c = vec3_scale(vec3_spherical(theta, phi + dphi), radius);
      vec3 d = vec3_scale(vec3_spherical(theta + dtheta, phi + dphi), radius);

      // Tri 0
      tri *t = &m->tris[ofs];
      *t = (tri){
        .v0 = a, .v1 = b, .v2 = c,
        .mtl = mtl
      };
      
      if(face_normals) {
        t->n0 = t->n1 = t->n2 = vec3_scale(vec3_unit(vec3_cross(vec3_sub(a, b), vec3_sub(c, b))), inv);
        t->face_nrm = 1.0f;
      } else {
        t->n0 = vec3_scale(a, inv * inv_r);
        t->n1 = vec3_scale(b, inv * inv_r);
        t->n2 = vec3_scale(c, inv * inv_r);
        t->face_nrm = 0.0f;
      }

      m->centers[ofs++] = tri_calc_center(t);

      // Tri 1
      t = &m->tris[ofs];
      *t = (tri){
        .v0 = a, .v1 = c, .v2 = d,
        .mtl = mtl
      };
      
      if(face_normals) {
        t->n0 = t->n1 = t->n2 = vec3_scale(vec3_unit(vec3_cross(vec3_sub(a, c), vec3_sub(d, c))), inv);
        t->face_nrm = 1.0f;
      } else {
        t->n0 = vec3_scale(a, inv * inv_r);
        t->n1 = vec3_scale(c, inv * inv_r);
        t->n2 = vec3_scale(d, inv * inv_r);
        t->face_nrm = 0.0f;
      }

      m->centers[ofs++] = tri_calc_center(t);

      phi += dphi;
    }

    theta += dtheta;
  }
}

void mesh_create_uvcylinder(mesh *m, float radius, float height, uint32_t subx, uint32_t suby, bool caps, uint32_t mtl, bool face_normals, bool invert_normals)
{
  mesh_init(m, 2 * subx * suby + (caps ? 2 * subx : 0));

  float dphi = 2 * PI / subx;
  float dh = height / suby;

  float inv_r = 1.0f / radius;

  float inv = invert_normals ? -1.0f : 1.0f;

  uint32_t ofs = 0;

  float h = 0.0f;
  for(uint32_t j=0; j<suby; j++) {
    float phi = 0.0f;
    for(uint32_t i=0; i<subx; i++) {
      float x0 = radius * sinf(phi);
      float x1 = radius * sinf(phi + dphi);
      float y0 = -height * 0.5f + h;
      float y1 = -height * 0.5f + h + dh;
      float z0 = radius * cosf(phi);
      float z1 = radius * cosf(phi + dphi);

      vec3 a = { x0, y1, z0 };
      vec3 b = { x0, y0, z0 };
      vec3 c = { x1, y0, z1 };
      vec3 d = { x1, y1, z1 };

      // Tri 0
      tri *t = &m->tris[ofs];
      *t = (tri){
        .v0 = a, .v1 = b, .v2 = c,
        .mtl = mtl
      };
      
      if(face_normals) {
        t->n0 = t->n1 = t->n2 = vec3_scale(vec3_unit(vec3_cross(vec3_sub(a, b), vec3_sub(c, b))), inv);
        t->face_nrm = 1.0f;
      } else {
        t->n0 = t->n1 = vec3_scale((vec3){ x0, 0.0f, z0 }, inv * inv_r);
        t->n2 = vec3_scale((vec3){ x1, 0.0f, z1 }, inv * inv_r);
        t->face_nrm = 0.0f;
      }

      m->centers[ofs++] = tri_calc_center(t);

      // Tri 1
      t = &m->tris[ofs];
      *t = (tri){
        .v0 = a, .v1 = c, .v2 = d,
        .mtl = mtl
      };
      
      if(face_normals) {
        t->n0 = t->n1 = t->n2 = vec3_scale(vec3_unit(vec3_cross(vec3_sub(a, c), vec3_sub(d, c))), inv);
        t->face_nrm = 1.0f;
      } else {
        t->n0 = vec3_scale((vec3){ x0, 0.0f, z0 }, inv * inv_r);
        t->n1 = t->n2 = vec3_scale((vec3){ x1, 0.0f, z1 }, inv * inv_r);
        t->face_nrm = 0.0f;
      }

      m->centers[ofs++] = tri_calc_center(t);

      phi += dphi;
    }

    h += dh;
  }

  if(caps) {
    float phi = 0.0f;
    for(uint32_t i=0; i<subx; i++) {
      float x0 = radius * sinf(phi);
      float x1 = radius * sinf(phi + dphi);
      float z0 = radius * cosf(phi);
      float z1 = radius * cosf(phi + dphi);

      // Tri 0 (top cap)
      tri *t = &m->tris[ofs];
      *t = (tri){
        .v0 = (vec3){ 0.0f, height * 0.5f, 0.0f }, .v1 = (vec3){ x0, height * 0.5f, z0 }, .v2 = (vec3){ x1, height * 0.5f, z1 },
        .n0 = (vec3){ 0.0f, inv * 1.0f, 0.0f}, .n1 = (vec3){ 0.0f, inv * 1.0f, 0.0f }, .n2 = (vec3){ 0.0f, inv * 1.0f, 0.0f },
        .mtl = mtl,
        .face_nrm = 1.0f
      };
      m->centers[ofs] = tri_calc_center(t);

      // Tri 1 (bottom cap)
      t = &m->tris[ofs + subx];
      *t = (tri){
        .v0 = (vec3){ 0.0f, -height * 0.5f, 0.0f }, .v1 = (vec3){ x1, -height * 0.5f, z1 }, .v2 = (vec3){ x0, -height * 0.5f, z0 },
        .n0 = (vec3){ 0.0f, inv * -1.0f, 0.0f}, .n1 = (vec3){ 0.0f, inv * -1.0f, 0.0f }, .n2 = (vec3){ 0.0f, inv * -1.0f, 0.0f },
        .mtl = mtl,
        .face_nrm = 1.0f
      };
      m->centers[ofs++] = tri_calc_center(t);

      phi += dphi;
    }
  }
}

void mesh_create_torus(mesh *m, float inner_radius, float outer_radius, uint32_t sub_inner, uint32_t sub_outer, uint32_t mtl, bool face_normals, bool invert_normals)
{
  mesh_init(m, 2 * sub_inner * sub_outer);

  float du = 2.0f * PI / sub_inner;
  float dv = 2.0f * PI / sub_outer;

  vec3  up = (vec3){ 0.0f, 1.0f, 0.0f };

  float inv_ir = 1.0f / inner_radius;

  float inv = invert_normals ? -1.0f : 1.0f;

  uint32_t ofs = 0;

  float v = 0.0f;
  for(uint32_t j=0; j<sub_outer; j++) {
    vec3 p0n = (vec3){ sinf(v), 0.0f, cosf(v) };
    vec3 p0 = vec3_scale(p0n, outer_radius);
    vec3 p1n = (vec3){ sinf(v + dv), 0.0f, cosf(v + dv) };
    vec3 p1 = vec3_scale(p1n, outer_radius);

    float u = 0.0f;
    for(uint32_t i=0; i<sub_inner; i++) {
      float x0 = sinf(u) * inner_radius;
      float y0 = cosf(u) * inner_radius;
      float x1 = sinf(u + du) * inner_radius;
      float y1 = cosf(u + du) * inner_radius;

      vec3 v0 = vec3_add(vec3_add(p0, vec3_scale(p0n, x0)), vec3_scale(up, y0));
      vec3 v1 = vec3_add(vec3_add(p0, vec3_scale(p0n, x1)), vec3_scale(up, y1));
      vec3 v2 = vec3_add(vec3_add(p1, vec3_scale(p1n, x0)), vec3_scale(up, y0));
      vec3 v3 = vec3_add(vec3_add(p1, vec3_scale(p1n, x1)), vec3_scale(up, y1));

      vec3 n0, n1, n2, n3;
      if(face_normals) {
        n0 = n1 = n2 = n3 = vec3_scale(vec3_cross(vec3_unit(vec3_sub(v1, v0)), vec3_unit(vec3_sub(v2, v0))), inv);
      } else {
        n0 = vec3_scale(vec3_sub(v0, p0), inv * inv_ir);
        n1 = vec3_scale(vec3_sub(v1, p0), inv * inv_ir);
        n2 = vec3_scale(vec3_sub(v2, p1), inv * inv_ir);
        n3 = vec3_scale(vec3_sub(v3, p1), inv * inv_ir);
      }

      // Tri 0
      tri *t = &m->tris[ofs];
      *t = (tri){
        .v0 = v0, .v1 = v1, .v2 = v3,
        .n0 = n0, .n1 = n1, .n2 = n3,
        .mtl = mtl,
        .face_nrm = face_normals ? 1.0f : 0.0f
      };
      m->centers[ofs++] = tri_calc_center(t);

      // Tri 1
      t = &m->tris[ofs];
      *t = (tri){
        .v0 = v0, .v1 = v3, .v2 = v2,
        .n0 = n0, .n1 = n3, .n2 = n2,
        .mtl = mtl,
        .face_nrm = face_normals ? 1.0f : 0.0f
      };
      m->centers[ofs++] = tri_calc_center(t);

      u += du;
    }
    v += dv;
  }
}

void mesh_create_icosphere(mesh *m, uint8_t steps, uint32_t mtl, bool face_normals, bool invert_normals)
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
    *t = (tri){
      .v0 = vec3_unit(t->v0),
      .v1 = vec3_unit(t->v1),
      .v2 = vec3_unit(t->v2)
    };
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

  // Materials, centers and normals
  float inv = invert_normals ? -1.0f : 1.0f;
  for(uint32_t i=0; i<m->tri_cnt; i++) {
    tri *t = &m->tris[i];
    t->mtl = mtl;
    m->centers[i] = tri_calc_center(t);
    if(face_normals) {
      vec3 n = vec3_scale(vec3_unit(m->centers[i]), inv);
      t->n0 = t->n1 = t->n2 = n;
      t->face_nrm = 1.0f;
    } else {
      t->n0 = vec3_scale(t->v0, inv);
      t->n1 = vec3_scale(t->v1, inv);
      t->n2 = vec3_scale(t->v2, inv);
      t->face_nrm = 0.0f;
    }
  }
}
