// rules/random_provider.h — источник случайности через интерфейс (E-D07, п. Г.4).
// DiceRoller не вызывает rand() напрямую: провайдер инжектируется.
#pragma once

#include <cstdint>

namespace rules {

class IRandomProvider {
public:
    virtual ~IRandomProvider() = default;
    virtual std::uint32_t next_u32() = 0;
    virtual int roll_d20() = 0; // 1..20
};

// Детерминированный генератор xoshiro256** (источник [25])
class SeededRandomProvider final : public IRandomProvider {
public:
    explicit SeededRandomProvider(std::uint64_t seed);
    std::uint32_t next_u32() override;
    int roll_d20() override;

private:
    std::uint64_t state_[4];
    std::uint64_t next();
};

} // namespace rules
