#ifndef PCGRAND_H
#define PCGRAND_H

#include <stdint.h>

void      pcg_srand(uint64_t seed, uint64_t seq);
uint32_t  pcg_rand(void);
float     pcg_randf(void);
float     pcg_randf_rng(float start, float end);

#endif
