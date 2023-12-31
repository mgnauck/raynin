#include "vec3.h"
#include "mutil.h"

vec3 vec3_rand()
{
  return (vec3){{{ randf(), randf(), randf() }}};
}

vec3 vec3_rand_rng(float start, float end)
{
  float d = end - start;
  return (vec3){{{ start + randf() * d, start + randf() * d, start + randf() * d }}};
}

vec3 vec3_add(vec3 a, vec3 b)
{
  return (vec3){{{ a.x + b.x, a.y + b.y, a.z + b.z }}};
}

vec3 vec3_sub(vec3 a, vec3 b)
{
  return (vec3){{{ a.x - b.x, a.y - b.y, a.z - b.z }}};
}

vec3 vec3_mul(vec3 a, vec3 b)
{
  return (vec3){{{ a.x * b.x, a.y * b.y, a.z * b.z }}};
}

vec3 vec3_neg(vec3 v)
{
  return (vec3){{{ -v.x, -v.y, -v.z }}};
}

vec3 vec3_scale(vec3 v, float s)
{
  return (vec3){{{ v.x * s, v.y * s, v.z * s }}};
}

vec3 vec3_cross(vec3 a, vec3 b)
{
  return (vec3){{{
    a.y * b.z - a.z * b.y,
    a.z * b.x - a.x * b.z,
    a.x * b.y - a.y * b.x }}};
}

vec3 vec3_unit(vec3 v)
{
  float il = 1.0f / vec3_len(v);
  return (vec3){{{ v.x * il, v.y * il, v.z * il }}};
}

float vec3_len(vec3 v)
{
  return sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
}

vec3 vec3_min(vec3 a, vec3 b)
{
  return (vec3){{{ min(a.x, b.x), min(a.y, b.y), min(a.z, b.z) }}};
}

vec3 vec3_max(vec3 a, vec3 b)
{
  return (vec3){{{ max(a.x, b.x), max(a.y, b.y), max(a.z, b.z) }}};
}

vec3 vec3_spherical(float theta, float phi)
{
  return (vec3){{{ -cosf(phi) * sinf(theta), -cosf(theta), sinf(phi) * sinf(theta) }}};
}
