#ifndef VEC3_H
#define VEC3_H

typedef struct vec3 {
  union {
    struct { float x, y, z; };
    float v[3];
  };
} vec3;

vec3 vec3_rnd();
vec3 vec3_rnd_rng(float min, float max);

vec3 vec3_add(vec3 a, vec3 b);
vec3 vec3_sub(vec3 a, vec3 b);
vec3 vec3_mul(vec3 a, vec3 b);

vec3 vec3_neg(vec3 v);
vec3 vec3_scale(vec3 v, float s);

vec3 vec3_cross(vec3 a, vec3 b);
vec3 vec3_unit(vec3 v);

float vec3_len(vec3 v);

vec3 vec3_min(vec3 a, vec3 b);
vec3 vec3_max(vec3 a, vec3 b);

vec3 vec3_spherical(float theta, float phi);

#endif
