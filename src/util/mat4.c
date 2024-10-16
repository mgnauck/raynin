#include "mat4.h"

#include <stdint.h>

#include "../base/log.h"
#include "../base/math.h"
#include "../base/string.h"
#include "../base/walloc.h"

void mat4_identity(mat4 d)
{
  d[0] = 1.0f;
  d[1] = 0.0f;
  d[2] = 0.0f;
  d[3] = 0.0f;
  d[4] = 0.0f;
  d[5] = 1.0f;
  d[6] = 0.0f;
  d[7] = 0.0f;
  d[8] = 0.0f;
  d[9] = 0.0f;
  d[10] = 1.0f;
  d[11] = 0.0f;
  d[12] = 0.0f;
  d[13] = 0.0f;
  d[14] = 0.0f;
  d[15] = 1.0f;
}

void mat4_transpose(mat4 d, const mat4 m)
{
  for(int j = 0; j < 4; j++)
    for(int i = 0; i < 4; i++)
      d[4 * j + i] = m[4 * i + j];
}

void mat4_copy(mat4 dst, const mat4 src)
{
  memcpy(dst, src, 16 * sizeof(float));
}

void mat4_trans(mat4 d, const vec3 v)
{
  d[0] = 1.0f;
  d[1] = 0.0f;
  d[2] = 0.0f;
  d[3] = v.x;
  d[4] = 0.0f;
  d[5] = 1.0f;
  d[6] = 0.0f;
  d[7] = v.y;
  d[8] = 0.0f;
  d[9] = 0.0f;
  d[10] = 1.0f;
  d[11] = v.z;
  d[12] = 0.0f;
  d[13] = 0.0f;
  d[14] = 0.0f;
  d[15] = 1.0f;
}

void mat4_rot_x(mat4 d, float rad)
{
  float c = cosf(rad);
  float s = sinf(rad);

  d[0] = 1.0f;
  d[1] = 0.0f;
  d[2] = 0.0f;
  d[3] = 0.0f;
  d[4] = 0.0f;
  d[5] = c;
  d[6] = -s;
  d[7] = 0.0f;
  d[8] = 0.0f;
  d[9] = s;
  d[10] = c;
  d[11] = 0.0f;
  d[12] = 0.0f;
  d[13] = 0.0f;
  d[14] = 0.0f;
  d[15] = 1.0f;
}

void mat4_rot_y(mat4 d, float rad)
{
  float c = cosf(rad);
  float s = sinf(rad);

  d[0] = c;
  d[1] = 0.0f;
  d[2] = s;
  d[3] = 0.0f;
  d[4] = 0.0f;
  d[5] = 1.0f;
  d[6] = 0.0f;
  d[7] = 0.0f;
  d[8] = -s;
  d[9] = 0.0f;
  d[10] = c;
  d[11] = 0.0f;
  d[12] = 0.0f;
  d[13] = 0.0f;
  d[14] = 0.0f;
  d[15] = 1.0f;
}

void mat4_rot_z(mat4 d, float rad)
{
  float c = cosf(rad);
  float s = sinf(rad);

  d[0] = c;
  d[1] = -s;
  d[2] = 0.0f;
  d[3] = 0.0f;
  d[4] = s;
  d[5] = c;
  d[6] = 0.0f;
  d[7] = 0.0f;
  d[8] = 0.0f;
  d[9] = 0.0f;
  d[10] = 1.0f;
  d[11] = 0.0f;
  d[12] = 0.0f;
  d[13] = 0.0f;
  d[14] = 0.0f;
  d[15] = 1.0f;
}

void mat4_scale(mat4 d, vec3 s)
{
  d[0] = s.x;
  d[1] = 0.0f;
  d[2] = 0.0f;
  d[3] = 0.0f;
  d[4] = 0.0f;
  d[5] = s.y;
  d[6] = 0.0f;
  d[7] = 0.0f;
  d[8] = 0.0f;
  d[9] = 0.0f;
  d[10] = s.z;
  d[11] = 0.0f;
  d[12] = 0.0f;
  d[13] = 0.0f;
  d[14] = 0.0f;
  d[15] = 1.0f;
}

void mat4_scale_u(mat4 d, float s)
{
  d[0] = s;
  d[1] = 0.0f;
  d[2] = 0.0f;
  d[3] = 0.0f;
  d[4] = 0.0f;
  d[5] = s;
  d[6] = 0.0f;
  d[7] = 0.0f;
  d[8] = 0.0f;
  d[9] = 0.0f;
  d[10] = s;
  d[11] = 0.0f;
  d[12] = 0.0f;
  d[13] = 0.0f;
  d[14] = 0.0f;
  d[15] = 1.0f;
}

void mat4_mul(mat4 d, const mat4 a, const mat4 b)
{
  mat4 t;
  t[0] = a[0] * b[0] + a[1] * b[4] + a[2] * b[8] + a[3] * b[12];
  t[1] = a[0] * b[1] + a[1] * b[5] + a[2] * b[9] + a[3] * b[13];
  t[2] = a[0] * b[2] + a[1] * b[6] + a[2] * b[10] + a[3] * b[14];
  t[3] = a[0] * b[3] + a[1] * b[7] + a[2] * b[11] + a[3] * b[15];

  t[4] = a[4] * b[0] + a[5] * b[4] + a[6] * b[8] + a[7] * b[12];
  t[5] = a[4] * b[1] + a[5] * b[5] + a[6] * b[9] + a[7] * b[13];
  t[6] = a[4] * b[2] + a[5] * b[6] + a[6] * b[10] + a[7] * b[14];
  t[7] = a[4] * b[3] + a[5] * b[7] + a[6] * b[11] + a[7] * b[15];

  t[8] = a[8] * b[0] + a[9] * b[4] + a[10] * b[8] + a[11] * b[12];
  t[9] = a[8] * b[1] + a[9] * b[5] + a[10] * b[9] + a[11] * b[13];
  t[10] = a[8] * b[2] + a[9] * b[6] + a[10] * b[10] + a[11] * b[14];
  t[11] = a[8] * b[3] + a[9] * b[7] + a[10] * b[11] + a[11] * b[15];

  t[12] = a[12] * b[0] + a[13] * b[4] + a[14] * b[8] + a[15] * b[12];
  t[13] = a[12] * b[1] + a[13] * b[5] + a[14] * b[9] + a[15] * b[13];
  t[14] = a[12] * b[2] + a[13] * b[6] + a[14] * b[10] + a[15] * b[14];
  t[15] = a[12] * b[3] + a[13] * b[7] + a[14] * b[11] + a[15] * b[15];

  for(uint8_t i = 0; i < 16; i++)
    d[i] = t[i];
}

vec3 mat4_mul_pos(const mat4 m, vec3 v)
{
  vec3 r = {
      m[0] * v.x + m[1] * v.y + m[2] * v.z + m[3],
      m[4] * v.x + m[5] * v.y + m[6] * v.z + m[7],
      m[8] * v.x + m[9] * v.y + m[10] * v.z + m[11],
  };

  float w = m[12] * v.x + m[13] * v.y + m[14] * v.z + m[15];

  return w == 1 ? r : vec3_scale(r, 1.0f / w);
}

vec3 mat4_mul_dir(const mat4 m, vec3 v)
{
  return (vec3){
      m[0] * v.x + m[1] * v.y + m[2] * v.z,
      m[4] * v.x + m[5] * v.y + m[6] * v.z,
      m[8] * v.x + m[9] * v.y + m[10] * v.z,
  };
}

// Taken from Mesa 3D
// https://stackoverflow.com/questions/1148309/inverting-a-4x4-matrix
bool mat4_inv(mat4 d, const mat4 m)
{
  mat4 inv;

  inv[0] = m[5] * m[10] * m[15] - m[5] * m[11] * m[14] - m[9] * m[6] * m[15] +
           m[9] * m[7] * m[14] + m[13] * m[6] * m[11] - m[13] * m[7] * m[10];

  inv[4] = -m[4] * m[10] * m[15] + m[4] * m[11] * m[14] + m[8] * m[6] * m[15] -
           m[8] * m[7] * m[14] - m[12] * m[6] * m[11] + m[12] * m[7] * m[10];

  inv[8] = m[4] * m[9] * m[15] - m[4] * m[11] * m[13] - m[8] * m[5] * m[15] +
           m[8] * m[7] * m[13] + m[12] * m[5] * m[11] - m[12] * m[7] * m[9];

  inv[12] = -m[4] * m[9] * m[14] + m[4] * m[10] * m[13] + m[8] * m[5] * m[14] -
            m[8] * m[6] * m[13] - m[12] * m[5] * m[10] + m[12] * m[6] * m[9];

  inv[1] = -m[1] * m[10] * m[15] + m[1] * m[11] * m[14] + m[9] * m[2] * m[15] -
           m[9] * m[3] * m[14] - m[13] * m[2] * m[11] + m[13] * m[3] * m[10];

  inv[5] = m[0] * m[10] * m[15] - m[0] * m[11] * m[14] - m[8] * m[2] * m[15] +
           m[8] * m[3] * m[14] + m[12] * m[2] * m[11] - m[12] * m[3] * m[10];

  inv[9] = -m[0] * m[9] * m[15] + m[0] * m[11] * m[13] + m[8] * m[1] * m[15] -
           m[8] * m[3] * m[13] - m[12] * m[1] * m[11] + m[12] * m[3] * m[9];

  inv[13] = m[0] * m[9] * m[14] - m[0] * m[10] * m[13] - m[8] * m[1] * m[14] +
            m[8] * m[2] * m[13] + m[12] * m[1] * m[10] - m[12] * m[2] * m[9];

  inv[2] = m[1] * m[6] * m[15] - m[1] * m[7] * m[14] - m[5] * m[2] * m[15] +
           m[5] * m[3] * m[14] + m[13] * m[2] * m[7] - m[13] * m[3] * m[6];

  inv[6] = -m[0] * m[6] * m[15] + m[0] * m[7] * m[14] + m[4] * m[2] * m[15] -
           m[4] * m[3] * m[14] - m[12] * m[2] * m[7] + m[12] * m[3] * m[6];

  inv[10] = m[0] * m[5] * m[15] - m[0] * m[7] * m[13] - m[4] * m[1] * m[15] +
            m[4] * m[3] * m[13] + m[12] * m[1] * m[7] - m[12] * m[3] * m[5];

  inv[14] = -m[0] * m[5] * m[14] + m[0] * m[6] * m[13] + m[4] * m[1] * m[14] -
            m[4] * m[2] * m[13] - m[12] * m[1] * m[6] + m[12] * m[2] * m[5];

  inv[3] = -m[1] * m[6] * m[11] + m[1] * m[7] * m[10] + m[5] * m[2] * m[11] -
           m[5] * m[3] * m[10] - m[9] * m[2] * m[7] + m[9] * m[3] * m[6];

  inv[7] = m[0] * m[6] * m[11] - m[0] * m[7] * m[10] - m[4] * m[2] * m[11] +
           m[4] * m[3] * m[10] + m[8] * m[2] * m[7] - m[8] * m[3] * m[6];

  inv[11] = -m[0] * m[5] * m[11] + m[0] * m[7] * m[9] + m[4] * m[1] * m[11] -
            m[4] * m[3] * m[9] - m[8] * m[1] * m[7] + m[8] * m[3] * m[5];

  inv[15] = m[0] * m[5] * m[10] - m[0] * m[6] * m[9] - m[4] * m[1] * m[10] +
            m[4] * m[2] * m[9] + m[8] * m[1] * m[6] - m[8] * m[2] * m[5];

  float det = m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12];

  if(det == 0)
    return false;

  det = 1.0 / det;

  for(uint8_t i = 0; i < 16; i++)
    d[i] = inv[i] * det;

  return true;
}

vec3 mat4_get_trans(const mat4 m)
{
  return (vec3){m[3], m[7], m[11]};
}

void mat4_from_row3x4(mat4 dst, const float *src)
{
  memcpy(dst, src, 12 * sizeof(*src));
  memcpy(dst + 12, &(float[4]){0.0f, 0.0f, 0.0f, 1.0f}, 4 * sizeof(float));
}

void mat4_from_quat(mat4 dst, float x, float y, float z, float w)
{
  float xx = x * x;
  float xy = x * y;
  float xz = x * z;
  float xw = x * w;
  float yy = y * y;
  float yz = y * z;
  float yw = y * w;
  float zz = z * z;
  float zw = z * w;
  dst[0] = 1.0f - 2.0f * (yy + zz);
  dst[1] = 2.0f * (xy - zw);
  dst[2] = 2.0f * (xz + yw);
  dst[4] = 2.0f * (xy + zw);
  dst[5] = 1.0f - 2.0f * (xx + zz);
  dst[6] = 2.0f * (yz - xw);
  dst[8] = 2.0f * (xz - yw);
  dst[9] = 2.0f * (yz + xw);
  dst[10] = 1.0f - 2.0f * (xx + yy);
  dst[3] = dst[7] = dst[11] = dst[12] = dst[13] = dst[14] = 0.0f;
  dst[15] = 1.0f;
}

void mat4_logc(mat4 m)
{
  for(uint8_t i = 0; i < 4; i++)
    logc("%6.3f %6.3f %6.3f %6.3f", m[4 * i], m[4 * i + 1], m[4 * i + 2],
         m[4 * i + 3]);
}
