#include "math.h"

float fabsf(float v)
{
  return v < 0 ? -v : v;
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
