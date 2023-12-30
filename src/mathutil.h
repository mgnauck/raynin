#ifndef MATHUTIL_H
#define MATHUTIL_H

#include <stdint.h>

// Beware of double evaluation
#define min(a, b) (((a) < (b)) ? (a) : (b))
#define max(a, b) (((a) > (b)) ? (a) : (b))

#define PI      3.141592654f
#define TWO_PI  6.283185307f
#define EPSILON 0.0001

extern float sqrtf(float a);
extern float sinf(float a);
extern float cosf(float a);
extern float acosf(float a);
extern float atan2f(float y, float x);
extern float powf(float base, float exp);

float fabsf(float v);
float floorf(float v);

void srand(uint64_t seed, uint64_t seq);

uint32_t rand(void);

float randf(void);
float randf_rng(float start, float end);

#endif
