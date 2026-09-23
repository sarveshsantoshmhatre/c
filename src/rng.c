#include "rng.h"

static uint64_t splitmix64(uint64_t* x) {
    uint64_t z = (*x += 0x9e3779b97f4a7c15ULL);
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31);
}

void sd_rng_seed(sd_rng* rng, uint64_t seed) {
    if (seed == 0) seed = 0x123456789abcdefULL;
    rng->state = splitmix64(&seed);
    if (rng->state == 0) rng->state = 0x6a09e667f3bcc909ULL;
}

uint64_t sd_rng_next_u64(sd_rng* rng) {
    uint64_t x = rng->state;
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    rng->state = x;
    return x * 2685821657736338717ULL;
}

uint32_t sd_rng_u32(sd_rng* rng) {
    return (uint32_t)(sd_rng_next_u64(rng) >> 32);
}

int sd_rng_range(sd_rng* rng, int min_value, int max_value) {
    if (max_value <= min_value) return min_value;
    uint32_t span = (uint32_t)(max_value - min_value + 1);
    return min_value + (int)(sd_rng_u32(rng) % span);
}

int sd_rng_chance(sd_rng* rng, int percent) {
    if (percent <= 0) return 0;
    if (percent >= 100) return 1;
    return sd_rng_range(rng, 1, 100) <= percent;
}
