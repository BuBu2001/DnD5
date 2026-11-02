// rules/damage.h — типы урона и сопротивления (п. Г.5).
#pragma once

#include <bitset>
#include <cstddef>
#include <cstdint>

namespace rules {

enum class DamageType : std::uint8_t {
    Bludgeoning, Piercing, Slashing, Fire, Cold, Lightning, Acid,
    Poison, Necrotic, Radiant, Force, Psychic, Thunder
};

inline constexpr std::size_t DamageTypeCount = 13;

struct DamageResistance {
    std::bitset<DamageTypeCount> resistant;
    std::bitset<DamageTypeCount> vulnerable;
    std::bitset<DamageTypeCount> immune;
};

// Сопротивление: floor(damage / 2); уязвимость: damage * 2;
// сопротивление и уязвимость одновременно: множитель × 1 (п. Г.5).
inline int apply_resistance(int damage, DamageType type, const DamageResistance& res) {
    const auto idx = static_cast<std::size_t>(type);
    if (res.immune.test(idx)) return 0;
    const bool r = res.resistant.test(idx);
    const bool v = res.vulnerable.test(idx);
    if (r && v) return damage;      // ×1
    if (r) return damage / 2;       // floor при целых неотрицательных
    if (v) return damage * 2;
    return damage;
}

} // namespace rules
