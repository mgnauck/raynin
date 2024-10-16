#include "tri.h"

#include "../base/math.h"

float tri_calc_area(vec3 v0, vec3 v1, vec3 v2)
{
  float a = vec3_len(vec3_sub(v2, v0));
  float b = vec3_len(vec3_sub(v1, v0));
  float c = vec3_len(vec3_sub(v2, v1));
  float s = 0.5 * (a + b + c);
  return sqrtf(s * (s - a) * (s - b) * (s - c));
}

void tri_build_ltri(ltri *lt, const tri *t, const tri_nrm *tn, mat4 transform,
                    mat4 inv_transform, vec3 emission)
{
  lt->v0 = mat4_mul_pos(transform, t->v0);
  lt->v1 = mat4_mul_pos(transform, t->v1);
  lt->v2 = mat4_mul_pos(transform, t->v2);

  // Assumes that emissive triangles have face normals, i.e. n0=n1=n2
  mat4 inv_t;
  mat4_transpose(inv_t, inv_transform);
  vec3 nrm = vec3_unit(mat4_mul_dir(inv_t, tn->n0));

  lt->nx = nrm.x;
  lt->ny = nrm.y;
  lt->nz = nrm.z;

  lt->area = tri_calc_area(lt->v0, lt->v1, lt->v2);
  lt->emission = emission;
}

void tri_update_ltri(ltri *lt, const tri *t, const tri_nrm *tn, mat4 transform,
                     mat4 inv_transform)
{
  lt->v0 = mat4_mul_pos(transform, t->v0);
  lt->v1 = mat4_mul_pos(transform, t->v1);
  lt->v2 = mat4_mul_pos(transform, t->v2);

  mat4 inv_t;
  mat4_transpose(inv_t, inv_transform);
  vec3 nrm = vec3_unit(mat4_mul_dir(inv_t, tn->n0));

  lt->nx = nrm.x;
  lt->ny = nrm.y;
  lt->nz = nrm.z;

  lt->area = tri_calc_area(lt->v0, lt->v1, lt->v2);
}
