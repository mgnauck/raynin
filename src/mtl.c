#include "mtl.h"
#include "mutil.h"

bool mtl_is_emissive(const mtl *mtl)
{
  return mtl->col.x > 1.0f || mtl->col.y > 1.0f || mtl->col.z > 1.0f;
}

mtl mtl_init(vec3 c)
{
  return (mtl){
    .col = c,
    .metallic = 0.0f,
    .roughness = 0.0f,
    .reflectance = 0.0f,
  };
}

mtl mtl_rand()
{
  return (mtl){
    .col = vec3_rand(),
    .metallic = pcg_randf(),
    .roughness = pcg_randf(),
    .reflectance = pcg_randf(),
  };
}
