#include "mesh.h"

#include "../base/math.h"
#include "../base/string.h"
#include "../base/walloc.h"
#include "tri.h"

void mesh_init(mesh *m, uint32_t tri_cnt)
{
  m->tri_cnt = tri_cnt;
  m->tris = malloc(tri_cnt * sizeof(*m->tris));
  m->tri_nrms = malloc(tri_cnt * sizeof(*m->tri_nrms));
  m->is_emissive = false;
  m->ofs = 0;
}

void mesh_release(mesh *m)
{
  free(m->tri_nrms);
  free(m->tris);
}

static void create_quad(tri *tris, tri_nrm *tri_nrms, vec3 pos, vec3 t, vec3 b,
                        uint8_t subx, uint8_t suby, uint16_t mtl,
                        bool invert_normals)
{
  float P = QUAD_DEFAULT_SIZE;
  float P2 = 0.5f * P;

  float dx = P / (float)subx;
  float dz = P / (float)suby;

  t = vec3_unit(t);
  b = vec3_unit(b);

  vec3 n = vec3_cross(b, t);
  vec3 n_final = vec3_scale(n, invert_normals ? -1.0f : 1.0f);

  vec3 o = vec3_add(vec3_add(pos, vec3_scale(t, -P2)), vec3_scale(b, -P2));

  tri *tp = tris;
  tri_nrm *tn = tri_nrms;

  float z = 0.0f;
  for(uint8_t j = 0; j < suby; j++) {
    float x = 0.0f;
    for(uint8_t i = 0; i < subx; i++) {
      // Tri 0
      *tp = (tri){
          .v0 = vec3_add(vec3_add(o, vec3_scale(t, x)), vec3_scale(b, z)),
          .v1 = vec3_add(vec3_add(o, vec3_scale(t, x)), vec3_scale(b, z + dz)),
          .v2 = vec3_add(vec3_add(o, vec3_scale(t, x + dx)),
                         vec3_scale(b, z + dz))};
      tn->n0 = tn->n1 = tn->n2 = n_final;
      tn->mtl = mtl;
      tp++;
      tn++;

      // Tri 1
      *tp = (tri){
          .v0 = vec3_add(vec3_add(o, vec3_scale(t, x)), vec3_scale(b, z)),
          .v1 = vec3_add(vec3_add(o, vec3_scale(t, x + dx)),
                         vec3_scale(b, z + dz)),
          .v2 = vec3_add(vec3_add(o, vec3_scale(t, x + dx)), vec3_scale(b, z))};
      tn->n0 = tn->n1 = tn->n2 = n_final;
      tn->mtl = mtl;
      tp++;
      tn++;

      x += dx;
    }

    z += dz;
  }
}

void mesh_create_quad(mesh *m, uint8_t subx, uint8_t suby, uint16_t mtl,
                      bool invert_normals)
{
  // Generate quad in XZ plane at origin with normal pointing up in Y
  mesh_init(m, 2 * subx * suby);
  create_quad(m->tris, m->tri_nrms, (vec3){0.0f, 0.0f, 0.0f},
              (vec3){1.0f, 0.0f, 0.0f}, (vec3){0.0f, 0.0f, 1.0f}, subx, suby,
              mtl, invert_normals);
}

void mesh_create_box(mesh *m, uint8_t subx, uint8_t suby, uint16_t mtl,
                     bool invert_normals)
{
  uint32_t cnt_per_side = 2 * subx * suby;
  mesh_init(m, 6 * cnt_per_side);

  // Top/bottom
  create_quad(m->tris, m->tri_nrms,
              (vec3){0.0f, QUAD_DEFAULT_SIZE * 0.5f, 0.0f},
              (vec3){1.0f, 0.0f, 0.0f}, (vec3){0.0f, 0.0f, 1.0f}, subx, suby,
              mtl, invert_normals);
  create_quad(m->tris + cnt_per_side, m->tri_nrms + cnt_per_side,
              (vec3){0.0f, -QUAD_DEFAULT_SIZE * 0.5f, 0.0f},
              (vec3){1.0f, 0.0f, 0.0f}, (vec3){0.0f, 0.0f, -1.0f}, subx, suby,
              mtl, invert_normals);

  // Left/right
  create_quad(m->tris + 2 * cnt_per_side, m->tri_nrms + 2 * cnt_per_side,
              (vec3){-QUAD_DEFAULT_SIZE * 0.5f, 0.0f, 0.0f},
              (vec3){0.0f, 1.0f, 0.0f}, (vec3){0.0f, 0.0f, 1.0f}, subx, suby,
              mtl, invert_normals);
  create_quad(m->tris + 3 * cnt_per_side, m->tri_nrms + 3 * cnt_per_side,
              (vec3){QUAD_DEFAULT_SIZE * 0.5f, 0.0f, 0.0f},
              (vec3){0.0f, -1.0f, 0.0f}, (vec3){0.0f, 0.0f, 1.0f}, subx, suby,
              mtl, invert_normals);

  // Front/back
  create_quad(m->tris + 4 * cnt_per_side, m->tri_nrms + 4 * cnt_per_side,
              (vec3){0.0f, 0.0f, QUAD_DEFAULT_SIZE * 0.5f},
              (vec3){1.0f, 0.0f, 0.0f}, (vec3){0.0f, -1.0f, 0.0f}, subx, suby,
              mtl, invert_normals);
  create_quad(m->tris + 5 * cnt_per_side, m->tri_nrms + 5 * cnt_per_side,
              (vec3){0.0f, 0.0f, -QUAD_DEFAULT_SIZE * 0.5f},
              (vec3){1.0f, 0.0f, 0.0f}, (vec3){0.0f, 1.0f, 0.0f}, subx, suby,
              mtl, invert_normals);
}

void mesh_create_sphere(mesh *m, float radius, uint8_t subx, uint8_t suby,
                        uint16_t mtl, bool face_normals, bool invert_normals)
{
  mesh_init(m, 2 * subx * suby);

  float dphi = 2 * PI / subx;
  float dtheta = PI / suby;

  float inv_r = 1.0f / radius;

  float inv = invert_normals ? -1.0f : 1.0f;

  tri *t = m->tris;
  tri_nrm *tn = m->tri_nrms;

  float theta = 0.0f;
  for(uint8_t j = 0; j < suby; j++) {
    float phi = 0.0f;
    for(uint8_t i = 0; i < subx; i++) {
      vec3 a = vec3_scale(vec3_spherical(theta + dtheta, phi), radius);
      vec3 b = vec3_scale(vec3_spherical(theta, phi), radius);
      vec3 c = vec3_scale(vec3_spherical(theta, phi + dphi), radius);
      vec3 d = vec3_scale(vec3_spherical(theta + dtheta, phi + dphi), radius);

      // Tri 0
      *t = (tri){.v0 = a, .v1 = b, .v2 = c};

      if(face_normals) {
        tn->n0 = tn->n1 = tn->n2 = vec3_scale(
            vec3_unit(vec3_cross(vec3_sub(c, b), vec3_sub(a, b))), inv);
      } else {
        tn->n0 = vec3_scale(a, inv * inv_r);
        tn->n1 = vec3_scale(b, inv * inv_r);
        tn->n2 = vec3_scale(c, inv * inv_r);
      }

      tn->mtl = mtl;

      t++;
      tn++;

      // Tri 1
      *t = (tri){.v0 = a, .v1 = c, .v2 = d};

      if(face_normals) {
        tn->n0 = tn->n1 = tn->n2 = vec3_scale(
            vec3_unit(vec3_cross(vec3_sub(d, c), vec3_sub(a, c))), inv);
      } else {
        tn->n0 = vec3_scale(a, inv * inv_r);
        tn->n1 = vec3_scale(c, inv * inv_r);
        tn->n2 = vec3_scale(d, inv * inv_r);
      }

      tn->mtl = mtl;

      t++;
      tn++;

      phi += dphi;
    }

    theta += dtheta;
  }
}

void mesh_create_cylinder(mesh *m, float radius, float height, uint8_t subx,
                          uint8_t suby, bool caps, uint16_t mtl,
                          bool face_normals, bool invert_normals)
{
  mesh_init(m, 2 * subx * suby + (caps ? 2 * subx : 0));

  float dphi = 2 * PI / subx;
  float dh = height / suby;

  float inv_r = 1.0f / radius;

  float inv = invert_normals ? -1.0f : 1.0f;

  tri *t = m->tris;
  tri_nrm *tn = m->tri_nrms;

  float h = 0.0f;
  for(uint8_t j = 0; j < suby; j++) {
    float phi = 0.0f;
    for(uint8_t i = 0; i < subx; i++) {
      float x0 = radius * sinf(phi);
      float x1 = radius * sinf(phi + dphi);
      float y0 = -height * 0.5f + h;
      float y1 = -height * 0.5f + h + dh;
      float z0 = radius * cosf(phi);
      float z1 = radius * cosf(phi + dphi);

      vec3 a = {x0, y1, z0};
      vec3 b = {x0, y0, z0};
      vec3 c = {x1, y0, z1};
      vec3 d = {x1, y1, z1};

      // Tri 0
      *t = (tri){.v0 = a, .v1 = b, .v2 = c};

      if(face_normals) {
        tn->n0 = tn->n1 = tn->n2 = vec3_scale(
            vec3_unit(vec3_cross(vec3_sub(c, b), vec3_sub(a, b))), inv);
      } else {
        tn->n0 = tn->n1 = vec3_scale((vec3){x0, 0.0f, z0}, inv * inv_r);
        tn->n2 = vec3_scale((vec3){x1, 0.0f, z1}, inv * inv_r);
      }

      tn->mtl = mtl;

      t++;
      tn++;

      // Tri 1
      *t = (tri){.v0 = a, .v1 = c, .v2 = d};

      if(face_normals) {
        tn->n0 = tn->n1 = tn->n2 = vec3_scale(
            vec3_unit(vec3_cross(vec3_sub(d, c), vec3_sub(a, c))), inv);
      } else {
        tn->n0 = vec3_scale((vec3){x0, 0.0f, z0}, inv * inv_r);
        tn->n1 = tn->n2 = vec3_scale((vec3){x1, 0.0f, z1}, inv * inv_r);
      }

      tn->mtl = mtl;

      t++;
      tn++;

      phi += dphi;
    }

    h += dh;
  }

  if(caps) {
    float phi = 0.0f;
    for(uint8_t i = 0; i < subx; i++) {
      float x0 = radius * sinf(phi);
      float x1 = radius * sinf(phi + dphi);
      float z0 = radius * cosf(phi);
      float z1 = radius * cosf(phi + dphi);

      // Tri 0 (top cap)
      *t = (tri){
          .v0 = (vec3){0.0f, height * 0.5f, 0.0f},
          .v1 = (vec3){x0, height * 0.5f, z0},
          .v2 = (vec3){x1, height * 0.5f, z1},
      };

      *tn = (tri_nrm){
          .n0 = (vec3){0.0f, inv * 1.0f, 0.0f},
          .n1 = (vec3){0.0f, inv * 1.0f, 0.0f},
          .n2 = (vec3){0.0f, inv * 1.0f, 0.0f},
          .mtl = mtl,
      };

      // Tri 1 (bottom cap)
      *(t + subx) = (tri){.v0 = (vec3){0.0f, -height * 0.5f, 0.0f},
                          .v1 = (vec3){x1, -height * 0.5f, z1},
                          .v2 = (vec3){x0, -height * 0.5f, z0}};

      *(tn + subx) = (tri_nrm){
          .n0 = (vec3){0.0f, inv * -1.0f, 0.0f},
          .n1 = (vec3){0.0f, inv * -1.0f, 0.0f},
          .n2 = (vec3){0.0f, inv * -1.0f, 0.0f},
          .mtl = mtl,
      };

      t++;
      tn++;

      phi += dphi;
    }
  }
}

void mesh_create_torus(mesh *m, float inner_radius, float outer_radius,
                       uint8_t sub_inner, uint8_t sub_outer, uint16_t mtl,
                       bool face_normals, bool invert_normals)
{
  mesh_init(m, 2 * sub_inner * sub_outer);

  float du = 2.0f * PI / sub_inner;
  float dv = 2.0f * PI / sub_outer;

  vec3 up = (vec3){0.0f, 1.0f, 0.0f};

  float inv_ir = 1.0f / inner_radius;

  float inv = invert_normals ? -1.0f : 1.0f;

  tri *t = m->tris;
  tri_nrm *tn = m->tri_nrms;

  float v = 0.0f;
  for(uint8_t j = 0; j < sub_outer; j++) {
    vec3 p0n = (vec3){sinf(v), 0.0f, cosf(v)};
    vec3 p0 = vec3_scale(p0n, outer_radius);
    vec3 p1n = (vec3){sinf(v + dv), 0.0f, cosf(v + dv)};
    vec3 p1 = vec3_scale(p1n, outer_radius);

    float u = 0.0f;
    for(uint8_t i = 0; i < sub_inner; i++) {
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
        n0 = n1 = n2 = n3 = vec3_scale(
            vec3_unit(vec3_cross(vec3_sub(v1, v0), vec3_sub(v2, v0))), inv);
      } else {
        n0 = vec3_scale(vec3_sub(v0, p0), inv * inv_ir);
        n1 = vec3_scale(vec3_sub(v1, p0), inv * inv_ir);
        n2 = vec3_scale(vec3_sub(v2, p1), inv * inv_ir);
        n3 = vec3_scale(vec3_sub(v3, p1), inv * inv_ir);
      }

      // Tri 0
      *t = (tri){.v0 = v0, .v1 = v1, .v2 = v3};
      *tn = (tri_nrm){.n0 = n0, .n1 = n1, .n2 = n3, .mtl = mtl};
      t++;
      tn++;

      // Tri 1
      *t = (tri){.v0 = v0, .v1 = v3, .v2 = v2};
      *tn = (tri_nrm){.n0 = n0, .n1 = n3, .n2 = n2, .mtl = mtl};
      t++;
      tn++;

      u += du;
    }
    v += dv;
  }
}
