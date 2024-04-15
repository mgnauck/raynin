#include "mtl.h"
#include "mutil.h"

bool mtl_is_emissive(const mtl *mtl)
{
  return mtl->col.x > 1.0f || mtl->col.y > 1.0f || mtl->col.z > 1.0f;
}

mtl mtl_create_diffuse()
{
  return (mtl){
    .col = vec3_mul(vec3_rand(), vec3_rand()),
    .fuzz = 1.0f, // Completely diffuse
    .trans = (vec3){ 0.0f, 0.0, 0.0f },
  };
}

mtl mtl_create_metal()
{
  return (mtl){
    .col = vec3_rand_rng(0.5f, 1.0f),
    .fuzz = 0.0f, // Perfect reflection
    .trans = (vec3){ 0.0f, 0.0, 0.0f },
 };
}

mtl mtl_create_dielectric()
{
  return (mtl){
    .col = (vec3){ 1.0f, 1.0f, 1.0f },
    .fuzz = 0.0f,
    .trans = (vec3){ 1.0f, 1.0f, 1.0f },
    .ior = 1.5f // Glass
 };
}

mtl mtl_create_emissive()
{
  return (mtl){ .col = (vec3){ 1.6f, 1.6f, 1.6f } };
}
