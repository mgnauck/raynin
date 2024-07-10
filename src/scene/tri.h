#ifndef TRI_H
#define TRI_H

#include "../util/mat4.h"
#include "../util/vec3.h"
#include "../settings.h"

#define MTL_ID_MASK 0xffff

typedef struct tri {
  vec3      v0;
  uint32_t  mtl;        // (mtl_id & 0xffff)
  vec3      v1;
  uint32_t  ltri_id;    // Set only if tri has light emitting material
  vec3      v2;
  float     face_nrm;   // 1.0f if tri has face normal
  vec3      n0;
  float     pad0;
  vec3      n1;
  float     pad1;
  vec3      n2;
  float     pad2;
} tri;

// Light tri
typedef struct ltri {
  vec3      v0;
  uint32_t  inst_id;    // (inst_id & 0xffff)
  vec3      v1;
  uint32_t  tri_id;     // Actual index into tri buf
  vec3      v2;
  float     area;
  vec3      nrm;
  float     power;
  vec3      emission;
  float     pad0;
} ltri;

float tri_calc_area(vec3 v0, vec3 v1, vec3 v2);
void  tri_build_ltri(ltri *lt, const tri *t, uint32_t inst_id, uint32_t tri_id, mat4 transform, vec3 emission);
void  tri_update_ltri(ltri *lt, const tri *t, mat4 transform);

#endif
