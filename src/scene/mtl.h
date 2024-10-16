#ifndef MTL_H
#define MTL_H

#include <stdbool.h>

#include "../util/vec3.h"

typedef struct mtl {
  vec3 col;         // Diffuse col of non-metallic/specular col of metallic
  float metallic;   // Appearance range from dielectric to conductor (0 - 1)
  float roughness;  // Perfect reflection to completely diffuse diffuse (0 - 1)
  float ior;        // Index of refraction
  float refractive; // Flag if material is refractive
  float emissive;   // Flag if material is emissive
} mtl;

mtl mtl_init(vec3 col);

#endif
