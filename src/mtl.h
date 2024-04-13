#ifndef MTL_H
#define MTL_H

#include "vec3.h"
#include <stdbool.h>

typedef struct mtl {
  vec3      col;    // Albedo/diffuse. If component > 1.0, obj emits light.
  float     fuzz;   // Fuzziness = 1 - glossiness
  vec3      trans;  // Transmittance per component
  float     ior;    // Refraction index of obj we are entering
} mtl;

bool  mtl_is_emissive(const mtl *mtl);

// Generate random materials
mtl   mtl_create_lambert();
mtl   mtl_create_metal();
mtl   mtl_create_dielectric();
mtl   mtl_create_emissive();

#endif
