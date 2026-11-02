#include "rules/random_provider.h"

#include <cstdio>

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

std::string SeededRandomProvider::export_state() const {
    std::string out;
    out.reserve(kStateWords * 16);
    char buf[17];
    for (std::size_t i = 0; i < kStateWords; ++i) {
        std::snprintf(buf, sizeof(buf), "%016llx",
                      static_cast<unsigned long long>(state_[i]));
        out += buf;
    }
    return out;
}

bool SeededRandomProvider::restore_state(const std::string& hex_state) {
    if (hex_state.size() != kStateWords * 16) return false;
    std::uint64_t parsed[kStateWords] = {0, 0, 0, 0};
    for (std::size_t w = 0; w < kStateWords; ++w) {
        std::uint64_t value = 0;
        for (std::size_t i = 0; i < 16; ++i) {
            const char c = hex_state[w * 16 + i];
            unsigned digit = 0;
            if (c >= '0' && c <= '9') digit = static_cast<unsigned>(c - '0');
            else if (c >= 'a' && c <= 'f') digit = static_cast<unsigned>(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') digit = static_cast<unsigned>(c - 'A' + 10);
            else return false; // недопустимый символ — состояние не изменяем
            value = (value << 4) | digit;
        }
        parsed[w] = value;
    }
    // Состояние xoshiro256** не должно быть нулевым (генератор вырождается)
    if (parsed[0] == 0 && parsed[1] == 0 && parsed[2] == 0 && parsed[3] == 0) return false;
    for (std::size_t i = 0; i < kStateWords; ++i) state_[i] = parsed[i];
    return true;
}

} // namespace rules
