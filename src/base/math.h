#ifndef MATH_H
#define MATH_H

#define EPS     1e-6
#define PI      3.14159265358979323846
#define TWO_PI  6.283185307179587

#define min(a, b) ((a) < (b) ? (a) : (b))
#define max(a, b) ((a) > (b) ? (a) : (b))

float fabsf(float value);
float fmodf(float x, float y);
float floorf(float x);

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

#endif
