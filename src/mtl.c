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
    //.col = c.x > 1.0f ? c : (vec3){ 1.0f, 1.0f, 1.0f },
    .metallic = 0.0f,
    .roughness = 0.0f,
    .reflectance = 0.0f,
    .pad0 = 0.0f,
    .pad1 = 0.0f
  };
}
