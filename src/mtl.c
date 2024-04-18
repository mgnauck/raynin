#include "mtl.h"
#include "mutil.h"

bool mtl_is_emissive(const mtl *mtl)
{
  return mtl->col.x > 1.0f || mtl->col.y > 1.0f || mtl->col.z > 1.0f;
}

mtl mtl_init()
{
  return (mtl){
    .col  = (vec3){ 0.0f, 0.0f, 0.0f },
    .refl = 0.0f,
    .refr = 0.0f,
    .fuzz = 0.0f,
    .ior  = 1.0f,
    .att  = (vec3){ 0.0f, 0.0f, 0.0f }
  };
}

mtl mtl_create_diffuse()
{
  mtl m = mtl_init();
  m.col = vec3_mul(vec3_rand(), vec3_rand());
  return m;
}

mtl mtl_create_mirror()
{
  mtl m = mtl_init();
  m.col = vec3_rand_rng(0.5f, 1.0f);
  m.refl = 1.0f;
  return m;
}

mtl mtl_create_glass()
{
  mtl m = mtl_init();
  m.col = (vec3){ 1.0f, 1.0f, 1.0f };
  m.refr = 1.0f;
  m.ior = 1.5f; // Glass
  return m;
}

mtl mtl_create_emissive()
{
  mtl m = mtl_init();
  m.col = (vec3){ 1.6f, 1.6f, 1.6f };
  return m;
}
