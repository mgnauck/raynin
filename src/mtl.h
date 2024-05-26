#ifndef MTL_H
#define MTL_H

#include "vec3.h"
#include <stdbool.h>

typedef struct mtl
{
  vec3  col;            // Diff col of non-metallics/spec col of metallics. Values > 1.0 = emissive.
  float metallic;       // Appearance range from dielectric to conductor (0 - 1)
  float roughness;      // Perfect reflection to completely diffuse diffuse (0 - 1)
  float reflectance;    // Reflectance for non-metallic materials (0 - 1)
  float pad0;
  float pad1;
} mtl;

bool  mtl_is_emissive(const mtl *mtl);
mtl   mtl_init(vec3 col);
mtl   mtl_rand();

#endif
