#ifndef MUTIL_H
#define MUTIL_H

#include <stdint.h>

#define PI      3.141592654f
#define TWO_PI  6.283185307f
#define EPSILON	0.0001f

// Beware of double evaluation
#define min(a, b) (((a) < (b)) ? (a) : (b))
#define max(a, b) (((a) > (b)) ? (a) : (b))

// Imported
extern float sqrtf(float a);
extern float sinf(float a);
extern float cosf(float a);
extern float tanf(float a);
extern float asinf(float a);
extern float acosf(float a);
extern float atan2f(float y, float x);
extern float powf(float base, float exp);
extern float fracf(float a);

float     fabsf(float v);
float     floorf(float v);
float     truncf(float v);
float     fmodf(float x, float y);

// PCG random
void      pcg_srand(uint64_t seed, uint64_t seq);
uint32_t  pcg_rand(void);
float     pcg_randf(void);
float     pcg_randf_rng(float start, float end);

#endif
