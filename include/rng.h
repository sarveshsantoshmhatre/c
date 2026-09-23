#ifndef SHATTERED_DEPTHS_RNG_H
#define SHATTERED_DEPTHS_RNG_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint64_t state;
} sd_rng;

void sd_rng_seed(sd_rng* rng, uint64_t seed);
uint32_t sd_rng_u32(sd_rng* rng);
int sd_rng_range(sd_rng* rng, int min_value, int max_value);
int sd_rng_chance(sd_rng* rng, int percent);
uint64_t sd_rng_next_u64(sd_rng* rng);

#ifdef __cplusplus
}
#endif

#endif
