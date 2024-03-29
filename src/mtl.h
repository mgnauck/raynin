#ifndef MTL_H
#define MTL_H

#include "vec3.h"
#include <stdbool.h>

#define MTL_TYPE_MASK   0xf

typedef enum mtl_type {
  MT_LAMBERT = 0,
  MT_METAL,
  MT_DIELECTRIC,
  MT_EMISSIVE
} mtl_type;

typedef struct mtl {
  vec3      color;      // Albedo/diffuse
  float     value;      // Glossiness or index of refraction
  vec3      emission;   // Light
  uint32_t  flags;      // Lowest 4 bits are mtl_type 
} mtl;

bool  mtl_is_emissive(const mtl *mtl);

// Generate random materials
mtl   mtl_create_lambert();
mtl   mtl_create_metal();
mtl   mtl_create_dielectric();
mtl   mtl_create_emissive();

#endif
