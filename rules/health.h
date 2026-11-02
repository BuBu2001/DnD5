// rules/health.h — состояние здоровья и порядок списания урона (п. 4.1.11.4, п. Г.3).
#pragma once

namespace rules {

struct HealthState {
    int hp_max = 0;
    int hp_current = 0;
    int temp_hp = 0;
    int death_saves_success = 0;
    int death_saves_failure = 0;
};

// Урон списывается сначала с temp_hp, затем с hp_current; лечение temp_hp не восстанавливает.
inline void apply_damage(HealthState& h, int damage) {
    if (damage <= 0) return;
    const int to_temp = damage < h.temp_hp ? damage : h.temp_hp;
    h.temp_hp -= to_temp;
    h.hp_current -= (damage - to_temp);
    if (h.hp_current < 0) h.hp_current = 0; // стабилизация/смерть —Rules Core, DOC-01
}

inline void heal(HealthState& h, int amount) {
    if (amount <= 0) return;
    h.hp_current += amount;
    if (h.hp_current > h.hp_max) h.hp_current = h.hp_max;
}

} // namespace rules
