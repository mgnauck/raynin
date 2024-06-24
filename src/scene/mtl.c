#include "mtl.h"
#include "../sys/mutil.h"

void mtl_set_defaults(mtl *m)
{
  // Blender defaults
  m->col = (vec3){ 1.0f, 1.0f, 1.0f };
  m->metallic = 0.0f;
  m->roughness = 0.5f;
  m->ior = 1.5f;
  m->refractive = 0.0f;
}

bool mtl_is_emissive(const mtl *mtl)
{
  return mtl->col.x > 1.0f || mtl->col.y > 1.0f || mtl->col.z > 1.0f;
}

mtl mtl_init(vec3 c)
{
  return (mtl){
    .col = c,
    .metallic = 0.0f,
    .roughness = 1.0f,
    .ior = 1.01f,
    .refractive = 0.0f
  };
}

mtl mtl_rand()
{
  return (mtl){
    .col = vec3_rand(),
    .metallic = pcg_randf(),
    .roughness = pcg_randf(),
    .ior = 1.01f,
    .refractive = 0.0f
  };
}
