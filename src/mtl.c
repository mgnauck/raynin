#include "mtl.h"
#include "mutil.h"

bool mtl_is_emissive(const mtl *mtl)
{
  return mtl->col.x > 1.0f || mtl->col.y > 1.0f || mtl->col.z > 1.0f;
}

mtl mtl_init(vec3 c)
{
  return (mtl){
    //.col = c.x > 1.0f ? c : (vec3){ 1.0f, 1.0f, 1.0f },
    .col = c,
    .metallic = 0.0f,
    .roughness = 1.0f,
    .reflectance = 0.0f,
    .clearCoat = 0.0f,
    .clearCoatRoughness = 1.0f
  };
}
