#ifndef TRI_H
#define TRI_H

#include "settings.h"
#include "vec3.h"

#define MTL_ID_MASK 0xffff

typedef struct tri {
  vec3      v0;
  uint32_t  mtl;    // (mtl_id & 0xffff)
  vec3      v1;
  float     pad1;
  vec3      v2;
  float     pad2;
  vec3      n0;
  float     pad3;
  vec3      n1;
  float     pad4;
  vec3      n2;
  float     pad5;
#ifdef TEXTURE_SUPPORT
  float     uv0[2];
  float     uv1[2];
  float     uv2[2];
  float     pad6;
  float     pad7;
#endif
} tri;

// Light tri
typedef struct ltri {
  vec3      v0;
  uint32_t  inst_id;
  vec3      v1;
  uint32_t  tri_id;
  vec3      v2;
  float     area;
  vec3      nrm;
  float     pad1;
  vec3      emission;
  float     pad2;
} ltri;

vec3  tri_calc_center(tri *t);
float tri_calc_area(vec3 v0, vec3 v1, vec3 v2);

void  tri_build_ltri(ltri *lt, const tri *t, uint32_t inst_id, uint32_t tri_id, float transform[12], vec3 emission);

#endif
