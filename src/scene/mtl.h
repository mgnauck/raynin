#ifndef MTL_H
#define MTL_H

#include <stdbool.h>
#include "../util/vec3.h"

typedef struct mtl
{
  vec3  col;            // Diff col of non-metallics/spec col of metallics. Values > 1.0 = emissive.
  float metallic;       // Appearance range from dielectric to conductor (0 - 1)
  float roughness;      // Perfect reflection to completely diffuse diffuse (0 - 1)
  float ior;            // Index of refraction
  float refractive;     // Flag if material refracts
  float pad0;
} mtl;

void  mtl_set_defaults(mtl *m);
bool  mtl_is_emissive(const mtl *mtl);

mtl   mtl_init(vec3 col);
mtl   mtl_rand();

#endif
