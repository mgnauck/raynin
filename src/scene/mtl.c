#include "mtl.h"

mtl mtl_init(vec3 c)
{
  return (mtl){.col = c,
               .metallic = 0.0f,
               .roughness = 0.5f,
               .ior = 1.5f,
               .refractive = 0.0f,
               .emissive = 0.0f};
}
