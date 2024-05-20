#ifndef MTL_H
#define MTL_H

#include "vec3.h"
#include <stdbool.h>

typedef struct mtl
{
  vec3  col;                // Diff col of non-metallics/spec col of metallics, values > 1.0 = emissive
  float metallic;           // Appearance range from dielectric to conductor (0 - 1)
  float roughness;          // Perfect reflection at 0, diffuse at 1
  float reflectance;        // Fresnel reflectance (F0) for non-metallic surfaces (mimics ior)
  float clearCoat;          // Strength of the clear coat layer
  float clearCoatRoughness; // Roughness of the clear coat layer
} mtl;

bool  mtl_is_emissive(const mtl *mtl);
mtl   mtl_init(vec3 col);

#endif
