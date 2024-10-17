#ifndef MAT4_H
#define MAT4_H

#include <stdbool.h>
#include "vec3.h"

// Standard math (row first) order:
//
// 0 1 2 3       1 0 0 x
// 4 5 6 7  and  0 1 0 y
// 8 9 0 a       0 0 1 z
// b c d e       0 0 0 1

typedef float mat4[16];

void mat4_identity(float *m);
void mat4_transpose(float *d, const float *m);

void mat4_trans(float *dest, const vec3 v);

void mat4_rot_x(float *dest, float radians);
void mat4_rot_y(float *dest, float radians);
void mat4_rot_z(float *dest, float radians);

void mat4_scale(float *dest, vec3 s);
void mat4_scale_u(float *dest, float s);

bool mat4_inv(float *dest, const float *src);

void mat4_mul(float *dest, const float *a, const float *b);

vec3 mat4_mul_pos(const float *m, const vec3 v);
vec3 mat4_mul_dir(const float *m, const vec3 v);

vec3 mat4_get_trans(const float *m);

void mat4_from_row3x4(float *dst, const float *src);
void mat4_from_quat(float *dst, float x, float y, float z, float w);

void mat4_logc(float *m);

#endif
