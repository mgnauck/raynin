#ifndef MUTIL_H
#define MUTIL_H

#include <stdint.h>
#include "settings.h"

#define PI      3.141592654f
#define TWO_PI  6.283185307f
#define EPSILON	0.0001f

// Beware of double evaluation
#define min(a, b) (((a) < (b)) ? (a) : (b))
#define max(a, b) (((a) > (b)) ? (a) : (b))

#ifndef NATIVE_BUILD

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

#else

#include <math.h>

#endif

// PCG random
void      pcg_srand(uint64_t seed, uint64_t seq);
uint32_t  pcg_rand(void);
float     pcg_randf(void);
float     pcg_randf_rng(float start, float end);

// Quasirandom sequence (LDS)
void      qrand_alpha(uint32_t dim, float *alpha);
void      qrand_init(uint32_t dim, uint32_t n, float *alpha, float *values);
void      qrand_get_next(uint32_t dim, float *alpha, float *last_values, float *new_values);
void      qrand_get_next_n(uint32_t dim, uint32_t n, float *alpha, float *last_values, float *new_values);

#endif
