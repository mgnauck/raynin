#include "mat.h"

lambert mat_create_lambert(vec3 albedo)
{
  return (lambert){ .albedo = albedo };
}

metal mat_create_metal(vec3 albedo, float fuzz_radius)
{
  return (metal){ .albedo = albedo, .fuzz_radius = fuzz_radius };
}

glass mat_create_glass(vec3 albedo, float refr_idx)
{
  return (glass){ .albedo = albedo, .refr_idx = refr_idx };
}
