#include "rules/random_provider.h"

namespace rules {

static std::uint64_t splitmix64(std::uint64_t& x) {
    std::uint64_t z = (x += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

static std::uint64_t rotl(std::uint64_t v, int k) {
    return (v << k) | (v >> (64 - k));
}

SeededRandomProvider::SeededRandomProvider(std::uint64_t seed) {
    std::uint64_t x = seed;
    for (int i = 0; i < 4; ++i) state_[i] = splitmix64(x);
}

std::uint64_t SeededRandomProvider::next() {
    const std::uint64_t result = rotl(state_[1] * 5, 7) * 9;
    const std::uint64_t t = state_[1] << 17;
    state_[2] ^= state_[0];
    state_[3] ^= state_[1];
    state_[1] ^= state_[2];
    state_[0] ^= state_[3];
    state_[2] ^= t;
    state_[3] = rotl(state_[3], 45);
    return result;
}

std::uint32_t SeededRandomProvider::next_u32() {
    return static_cast<std::uint32_t>(next() >> 32);
}

int SeededRandomProvider::roll_d20() {
    return static_cast<int>(next_u32() % 20u) + 1;
}

} // namespace rules
