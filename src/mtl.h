#ifndef MTL_H
#define MTL_H

#include "vec3.h"
#include <stdbool.h>

typedef struct mtl {
  vec3      col;    // Albedo/diffuse. If component > 1.0, obj emits light.
  float     refl;   // Probability of reflection (diffuse = 1 - refl - refr)
  float     refr;   // Probability of refraction
  float     fuzz;   // Fuzziness = 1 - glossiness
  float     ior;    // Refraction index of obj we are entering
  float     pad0;
  vec3      att;    // Attenuation of transmission per col comp (beer)
  float     pad1;
} mtl;

bool  mtl_is_emissive(const mtl *mtl);

mtl   mtl_init(vec3 col);

// Generate random materials
mtl   mtl_create_diffuse();
mtl   mtl_create_mirror();
mtl   mtl_create_glass();
mtl   mtl_create_emissive();

#endif
