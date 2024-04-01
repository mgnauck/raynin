#include "tri.h"
#include "mat4.h"
#include "mutil.h"

vec3 tri_calc_center(tri *t)
{
  return vec3_scale(vec3_add(vec3_add(t->v0, t->v1), t->v2), 0.3333f);
}

float tri_calc_area(vec3 v0, vec3 v1, vec3 v2)
{
  float a = vec3_len(vec3_sub(v2, v0));
  float b = vec3_len(vec3_sub(v1, v0));
  float c = vec3_len(vec3_sub(v2, v1));
  float s = 0.5 * (a + b + c);
  return sqrtf(s * (s - a) * (s - b) * (s - c));
}

void tri_build_ltri(ltri *lt, const tri *t, uint32_t inst_id, uint32_t tri_id, float transform[12], vec3 emission)
{
  mat4 trans;
  mat4_from_row3x4(trans, transform);

  lt->v0 = mat4_mul_pos(trans, t->v0);
  lt->v1 = mat4_mul_pos(trans, t->v1);
  lt->v2 = mat4_mul_pos(trans, t->v2);
  
  // Assumes that emissive triangles have face normals, i.e. n0=n1=n2
  lt->nrm = vec3_unit(mat4_mul_dir(trans, t->n0));
  
  lt->inst_id = inst_id;
  lt->tri_id = tri_id;
  
  lt->area = tri_calc_area(lt->v0, lt->v1, lt->v2);
  
  lt->emission = emission;
}
