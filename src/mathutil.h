#ifndef MATHUTIL_H
#define MATHUTIL_H

#include <stddef.h>

/*
#define max(a, b)            \
({                           \
    __typeof__ (a) _a = (a); \
    __typeof__ (b) _b = (b); \
    _a > _b ? _a : _b;       \
})

#define min(a, b)            \
({                           \
    __typeof__ (a) _a = (a); \
    __typeof__ (b) _b = (b); \
    _a < _b ? _a : _b;       \
})
*/

// Does not prevent double evaluation but above macros give GNU
// statement expression extension errors on designated initializers
#define min(a, b) (((a) < (b)) ? (a) : (b))
#define max(a, b) (((a) > (b)) ? (a) : (b))

#define PI      3.141592654f
#define TWO_PI  6.283185307f

extern float sqrtf(float a);
extern float sinf(float a);
extern float cosf(float a);
extern float acosf(float a);
extern float atan2f(float y, float x);
extern float powf(float base, float exp);

float floorf(float v);

#endif
