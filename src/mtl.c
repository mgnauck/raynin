#include "mtl.h"
#include "mutil.h"

bool mtl_is_emissive(const mtl *mtl)
{
  return (mtl->flags & 0xf) == MT_EMISSIVE;
  //return mtl->emission.x > 0.0f || mtl->emission.y > 0.0f || mtl->emission.z > 0.0f;
}

mtl mtl_create_lambert()
{
  return (mtl){
    .color = vec3_mul(vec3_rand(), vec3_rand()),
    .value = 0.0f,
    .emission = (vec3){ 0.0f, 0.0f, 0.0f },
    .flags = MT_LAMBERT
  };
}

mtl mtl_create_metal()
{
  return (mtl){
    .color = vec3_rand_rng(0.5f, 1.0f),
    .value = pcg_randf_rng(0.001f, 0.5f),
    .emission = (vec3){ 0.0f, 0.0f, 0.0f },
    .flags = MT_METAL
  };
}

mtl mtl_create_dielectric()
{
  return (mtl){
    .color = (vec3){ 1.0f, 1.0f, 1.0f },
    .value = 1.5f, // glass
    .emission = (vec3){ 0.0f, 0.0f, 0.0f },
    .flags = MT_DIELECTRIC
  };
}

mtl mtl_create_emissive()
{
  return (mtl){
    .color = (vec3){ 0.0f, 0.0f, 0.0f },
    .value = 0.0f,
    .emission = (vec3){ 1.6f, 1.6f, 1.6f },
    .flags = MT_EMISSIVE
  };
}
