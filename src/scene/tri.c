#include "tri.h"
#include "../sys/mutil.h"

float tri_calc_area(vec3 v0, vec3 v1, vec3 v2)
{
  float a = vec3_len(vec3_sub(v2, v0));
  float b = vec3_len(vec3_sub(v1, v0));
  float c = vec3_len(vec3_sub(v2, v1));
  float s = 0.5 * (a + b + c);
  return sqrtf(s * (s - a) * (s - b) * (s - c));
}

void tri_build_ltri(ltri *lt, const tri *t, uint32_t inst_id, uint32_t tri_id, mat4 transform, vec3 emission)
{
  lt->v0 = mat4_mul_pos(transform, t->v0);
  lt->v1 = mat4_mul_pos(transform, t->v1);
  lt->v2 = mat4_mul_pos(transform, t->v2);
  
  // Assumes that emissive triangles have face normals, i.e. n0=n1=n2
  lt->nrm = vec3_unit(mat4_mul_dir(transform, t->n0));
  
  lt->inst_id = inst_id;
  lt->tri_id = tri_id;
  
  lt->area = tri_calc_area(lt->v0, lt->v1, lt->v2);
  lt->emission = emission;
  lt->power = lt->area * (emission.x + emission.y + emission.z);
}

void tri_update_ltri(ltri *lt, const tri *t, mat4 transform)
{
  // TODO Check if product from current transform and previous inverse is
  // faster than fetching the original triangle data
  lt->v0 = mat4_mul_pos(transform, t->v0);
  lt->v1 = mat4_mul_pos(transform, t->v1);
  lt->v2 = mat4_mul_pos(transform, t->v2);
  
  lt->nrm = vec3_unit(mat4_mul_dir(transform, t->n0));

  lt->area = tri_calc_area(lt->v0, lt->v1, lt->v2);
  lt->power = lt->area * (lt->emission.x + lt->emission.y + lt->emission.z);
}
