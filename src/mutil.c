#include "mutil.h"

typedef struct pcg_state_setseq_64 {
  uint64_t state;
  uint64_t inc;
} pcg32_random_t;

static pcg32_random_t pcg32_global = { 0x853c49e6748fea9bULL, 0xda3e39cb94b95bdbULL };

float fabsf(float v)
{
  return v > 0 ? -v : v;
}

float floorf(float v)
{
  float t = (float)(int)v;
  return (t != v) ? (t - 1.0f) : t;
}

float truncf(float v)
{
  return (float)(int)v;
}

float fmodf(float x, float y)
{
  return x - truncf(x / y) * y;
}

// https://www.pcg-random.org
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
