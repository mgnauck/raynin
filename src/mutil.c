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

#ifdef NATIVE_BUILD
float fracf(float a)
{
  float integer;
  return modff(a, &integer);
}
#endif

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

// https://extremelearning.com.au/unreasonable-effectiveness-of-quasirandom-sequences/
float qrand_phi(uint32_t dim)
{
  float x = 2.0f;
  for(uint32_t i=0; i<10; i++)
    x = powf(1.0f + x, 1.0f / ((float)dim + 1.0f));
  return x;
}

void qrand_alpha(uint32_t dim, float *alpha)
{
  // Expects alpha to be of size dim
  float ginv = 1.0f / qrand_phi(dim);
  for(uint32_t i=0; i<dim; i++)
    alpha[i] = fracf(powf(ginv, (float)i + 1.0f));
}

void qrand_init(uint32_t dim, uint32_t n, float *alpha, float *values)
{
  // Expects values to be of size dim * n
  if(n < 1)
    return;

  float seed = 0.5f;
  for(uint32_t i=0; i<dim; i++)
    values[i] = seed;

  for(uint32_t j=1; j<n; j++)
    qrand_get_next(dim, alpha, &values[dim * (j - 1)], &values[dim * j]);
}

void qrand_get_next(uint32_t dim, float *alpha, float *last_values, float *new_values)
{
  // Expects last_values and new_values to be of size dim
  for(uint32_t i=0; i<dim; i++)
    new_values[i] = fracf(last_values[i] + alpha[i]);
}

void qrand_get_next_n(uint32_t dim, uint32_t n, float *alpha, float *last_values, float *new_values)
{
  // Expects last_values to be of size dim and new_values to be of size dim * n
  if(n < 1)
    return;

  qrand_get_next(dim, alpha, last_values, new_values);

  for(uint32_t j=1; j<n; j++)
    qrand_get_next(dim, alpha, &new_values[dim * (j - 1)], &new_values[dim * j]);
}
