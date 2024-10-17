
#include "math.h"

uint32_t rand_uint32()
{
    static uint32_t randSeed = 22222;
    randSeed = (randSeed * 196314165) + 907633515;
    return randSeed;
}

float rand_float()
{
    return (float)rand_uint32() / (float)(UINT32_MAX);
}

float clamp(float value, float min, float max)
{
    return value < min ? min : value > max ? max
                                           : value;
}

// credits: https://stackoverflow.com/questions/71336291/fastest-way-to-get-square-root-in-float-value
float sqrtf(float value)
{
    int32_t i;
    for (i = 0; i * i <= (int32_t)value; i++)
        ;

    float lb = (float)i - 1.f; // lower bound
    if (lb * lb == value)
        return lb;
    float ub = lb + 1; // upper bound
    float pub = ub;    // previous upper bound
    for (int j = 0; j <= 20; j++)
    {
        float ub2 = ub * ub;
        if (ub2 > value)
        {
            pub = ub;
            ub = (lb + ub) / 2; // mid value of lower and upper bound
        }
        else
        {
            lb = ub;
            ub = pub;
        }
    }
    return ub;
}

float fabsf(float value)
{
    return value < 0.f ? -value : value;
}

/*
// doesn't work with negative values but is fast
float fmodf(float x, float y)
{
    return x - y * (int)(x / y);
}
*/

// below works with negative values as well
float fmodf(float x, float y)
{
    return x - y * floorf(x / y);
}

// credits: https://www.youtube.com/watch?v=1xlCVBIF_ig
float sinf_approx(float value)
{
    float t = value * 0.15915f;

    t = t - (float)((int32_t)t);

    return 20.785f * t * (t - 0.5f) * (t - 1.f);
}

float cosf_approx(float value)
{
    return sinf_approx(value + PI2); // Adding PI/2 to the input value
}

float floorf(float v)
{
    float t = (float)(int)v;
    return (t != v) ? (t - 1.0f) : t;
}

static const float exp2f_table[16] = {
    1.000000000f, 1.044273782f, 1.090507733f, 1.138788635f,
    1.189207115f, 1.241857812f, 1.296839555f, 1.354255547f,
    1.414213562f, 1.476826146f, 1.542210825f, 1.610490331f,
    1.681792831f, 1.756252161f, 1.834008086f, 1.915206561f};

float exp2f(float x)
{
    union
    {
        float f;
        uint32_t i;
    } u;
    float t, r, y;
    int32_t n, j;

    // Get the integer part and the fractional part of x
    n = (int32_t)floorf(x);
    x -= (float)n;

    // Scale n to the exponent range
    u.i = (uint32_t)((n + 127) << 23);

    // Polynomial approximation of 2^x for the fractional part
    j = (int32_t)(x * 16.0f);
    t = x - (float)j * 0.0625f;

    // Use a table lookup for the fractional part
    y = exp2f_table[j];

    // Polynomial for exp2f(t) where |t| <= 1/32
    r = 0.9999999916728642f + t * (0.6931471805599453f + t * (0.2402265069591007f + t * (0.0555041086648216f + t * (0.0096181291076285f + t * 0.0013333558146428f))));

    // Combine the integer and fractional parts
    return u.f * y * r;
}

float tanhf_approx(float value)
{
    // Clamp the input value for stability
    if (value > 10.0f)
        return 1.0f;
    if (value < -10.0f)
        return -1.0f;

    // Use exp2f to calculate e^(2x)
    float e2x = exp2f(2.f * value * (float)LOG2E);
    return (e2x - 1) / (e2x + 1);
}

float tanf_approx(float x)
{
    // Polynomial approximation for tan(x)
    return x + (x * x * x) / 3.f;
}

float tanf(float x)
{
    // Reduce the angle to the range [-PI, PI] for better precision
    while (x > (float)PI)
    {
        x -= (float)TAU;
    }
    while (x < (float)-PI)
    {
        x += (float)TAU;
    }

    // Reduce the angle to the range [-PI/2, PI/2]
    if (x > (float)PI / 2.f)
    {
        x = (float)PI - x;
    }
    else if (x < (float)-PI / 2.f)
    {
        x = (float)-PI - x;
    }

    // Pade approximation for tan(x)
    float x2 = x * x;
    return x * (15.f + x2) / (15.f - x2);
}

float atanf_approx(float x)
{
    // Constants for the approximation
    const float a = 0.280872f;

    // Compute approximation
    return x / (1.f + a * x * x);
}

float noteToFreq(float note)
{
    return 440.f * exp2f((note - 69.f) * (1.f / 12.f));
}

typedef struct pcg_state_setseq_64 {
  uint64_t state;
  uint64_t inc;
} pcg32_random_t;

static pcg32_random_t pcg32_global = { 0x853c49e6748fea9bULL, 0xda3e39cb94b95bdbULL };

// https://www.pcg-random.org/
uint32_t pcg32_random_r(pcg32_random_t *rng)
{
  uint64_t oldstate = rng->state;
  rng->state = oldstate * 6364136223846793005ULL + rng->inc;
  uint32_t xorshifted = ((oldstate >> 18u) ^ oldstate) >> 27u;
  uint32_t rot = oldstate >> 59u;
  return (xorshifted >> rot) | (xorshifted << ((-rot) & 31));
}

void pcg32_srandom_r(pcg32_random_t* rng, uint64_t initstate, uint64_t initseq)
{
    rng->state = 0U;
    rng->inc = (initseq << 1u) | 1u;
    pcg32_random_r(rng);
    rng->state += initstate;
    pcg32_random_r(rng);
}

void pcg_srand(uint64_t seed, uint64_t seq)
{
  pcg32_srandom_r(&pcg32_global, seed, seq);
}

uint32_t pcg_rand()
{
  return pcg32_random_r(&pcg32_global);
}

float pcg_randf()
{
  // ldexp(rand(), -32);
  return pcg_rand() / (double)UINT32_MAX;
}

float pcg_randf_rng(float start, float end)
{
  return start + pcg_randf() * (end - start);
}
