#include "shape.h"

sphere shape_create_sphere(vec3 center, float radius)
{
  return (sphere){ .center = center, .radius = radius };
}

quad shape_create_quad(vec3 q, vec3 u, vec3 v)
{
  return (quad){ .q = q, .u = u, .v = v };
}
