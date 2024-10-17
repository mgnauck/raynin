#include "math.h"

float fabsf(float x)
{
  return x < 0 ? -x : x;
}

float floorf(float v)
{
  float t = (float)(int)v;
  return (t != v) ? (t - 1.0f) : t;
}

float fmodf(float x, float y)
{
  return x - floorf(x / y) * y;
}
