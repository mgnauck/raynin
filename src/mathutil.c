#include "mathutil.h"
#include <stdint.h>

typedef struct pcg_state_setseq_64 {
  uint64_t state;
  uint64_t inc;
} pcg32_random_t;

static pcg32_random_t pcg32_global = { 0x853c49e6748fea9bULL, 0xda3e39cb94b95bdbULL };

float floorf(float v)
{
  float t = (float)(int)v;
  return (t != v) ? (t - 1.0f) : t;
}

// https://www.pcg-random.org
uint32_t pcg32_random_r(pcg32_random_t* rng)
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

void srand(uint64_t seed, uint64_t seq)
{
  pcg32_srandom_r(&pcg32_global, seed, seq);
}

uint32_t rand()
{
  return pcg32_random_r(&pcg32_global);
}

float randf()
{
  // ldexp(rand(), -32);
  return rand() / (double)UINT32_MAX;
}
