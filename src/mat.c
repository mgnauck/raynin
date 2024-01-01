#include "mat.h"

lambert lambert_create(vec3 albedo)
{
  return (lambert){ .albedo = albedo };
}

metal metal_create(vec3 albedo, float fuzz_radius)
{
  return (metal){ .albedo = albedo, .fuzz_radius = fuzz_radius };
}

glass glass_create(vec3 albedo, float refr_idx)
{
  return (glass){ .albedo = albedo, .refr_idx = refr_idx };
}
