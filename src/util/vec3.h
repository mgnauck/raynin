#ifndef VEC3_H
#define VEC3_H

#include <stdint.h>

typedef struct vec3 {
  float x, y, z;
} vec3;

void vec3_set(vec3 *v, uint8_t idx, float val);
float vec3_get(vec3 v, uint8_t idx);

vec3 vec3_rand();
vec3 vec3_rand_rng(float min, float max);

vec3 vec3_rand2_disk();

vec3 vec3_add(vec3 a, vec3 b);
vec3 vec3_sub(vec3 a, vec3 b);
vec3 vec3_mul(vec3 a, vec3 b);

vec3 vec3_neg(vec3 v);
vec3 vec3_scale(vec3 v, float s);

vec3 vec3_cross(vec3 a, vec3 b);
vec3 vec3_unit(vec3 v);

float vec3_dot(vec3 a, vec3 b);
float vec3_len(vec3 v);

vec3 vec3_min(vec3 a, vec3 b);
vec3 vec3_max(vec3 a, vec3 b);

float vec3_min_comp(vec3 v);
float vec3_max_comp(vec3 v);

vec3 vec3_abs(vec3 v);

vec3 vec3_uint32(uint32_t v);
vec3 vec3_spherical(float theta, float phi);

void vec3_logc(const char *prefix, vec3 v);

#endif
