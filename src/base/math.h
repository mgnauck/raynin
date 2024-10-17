
#ifndef __MATH_H
#define __MATH_H

#include <stdint.h>

#define EPSILON 1e-6
#define PI 3.14159265358979323846
#define PI2 1.5707963267948966
#define TAU 6.283185307179587
#define LOG2E 1.44269504088896340736 // log2(e)
#define LOG210 3.321928              // log base 2 of 10

#define min(a, b) ((a) < (b) ? (a) : (b))
#define max(a, b) ((a) > (b) ? (a) : (b))

// imported
extern float sqrtf(float a);
extern float sinf(float a);
extern float cosf(float a);
extern float tanf(float a);
extern float asinf(float a);
extern float acosf(float a);
extern float atan2f(float y, float x);
extern float powf(float base, float exp);
extern float fracf(float a);

uint32_t rand_uint32();
// rand_double();
float rand_float();
float clamp(float value, float min, float max);
float sqrtf(float value);
float fabsf(float value);
float fmodf(float x, float y);
float sinf_approx(float value);
float cosf_approx(float value);
float floorf(float x);
float exp2f(float value);
float tanhf_approx(float value);
float tanf_approx(float x);
float tanf(float x);
float atanf_approx(float x);
float noteToFreq(float note);

// TODO consolidate with rand_uint32
// PCG random
void pcg_srand(uint64_t seed, uint64_t seq);
uint32_t pcg_rand(void);
float pcg_randf(void);
float pcg_randf_rng(float start, float end);

#endif
