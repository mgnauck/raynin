#include "vec3.h"
#include "mathutil.h"
#include "text.h"

vec3 vec3_rnd()
{
  // TODO
  return (vec3){ .x = 1.0f, .y = 1.0f, .z = 1.0f };
}

vec3 vec3_rnd_rng(float min, float max)
{
  // TODO
  return (vec3){ .x = 1.0f, .y = 1.0f, .z = 1.0f };
}

vec3 vec3_add(vec3 a, vec3 b)
{
  return (vec3){ .x = a.x + b.x, .y = a.y + b.y, .z = a.z + b.z };
}

vec3 vec3_sub(vec3 a, vec3 b)
{
  return (vec3){ .x = a.x - b.x, .y = a.y - b.y, .z = a.z - b.z };
}

vec3 vec3_mul(vec3 a, vec3 b)
{
  return (vec3){ .x = a.x * b.x, .y = a.y * b.y, .z = a.z * b.z };
}

vec3 vec3_neg(vec3 v)
{
  return (vec3){ .x = -v.x, .y = -v.y, .z = -v.z };
}

vec3 vec3_scale(vec3 v, float s)
{
  return (vec3){ .x = v.x * s, .y = v.y * s, .z = v.z * s };
}

vec3 vec3_cross(vec3 a, vec3 b)
{
  return (vec3){ 
    .x = a.y * b.z - a.z * b.y,
    .y = a.z * b.x - a.x * b.z,
    .z = a.x * b.y - a.y * b.x };
}

vec3 vec3_unit(vec3 v)
{
  float il = 1.0f / vec3_len(v);
  return (vec3){ .x = v.x * il, .y = v.y * il, .z = v.z * il };
}

float vec3_len(vec3 v)
{
  return sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
}

vec3 vec3_min(vec3 a, vec3 b)
{
  return (vec3){ .x = min(a.x, b.x), .y = min(a.y, b.y), .z = min(a.z, b.z) };
}

vec3 vec3_max(vec3 a, vec3 b)
{
  return (vec3){ .x = max(a.x, b.x), .y = max(a.y, b.y), .z = max(a.z, b.z) };
}

vec3 vec3_spherical(float theta, float phi)
{
  return (vec3){ .x = -cosf(phi) * sinf(theta), .y = -cosf(theta), .z = sinf(phi) * sinf(theta) };
}

size_t push_vec3(vec3 v, uint8_t precision, size_t ofs)
{
  size_t cnt = push_float(v.x, precision, ofs);
  cnt = push_str(", ", cnt);
  cnt = push_float(v.y, precision, cnt);
  cnt = push_str(", ", cnt);
  return push_float(v.z, precision, cnt);
}

void print_vec3(vec3 v, uint8_t precision)
{
  print_buf(push_vec3(v, precision, 0));
}

void text_vec3(vec3 v)
{
  global_str_buf_pos = push_vec3(v, 6, global_str_buf_pos);
}

void text_vec3_p(vec3 v, uint8_t precision)
{
  global_str_buf_pos = push_vec3(v, precision, global_str_buf_pos);
}

