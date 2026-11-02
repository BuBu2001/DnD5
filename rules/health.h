// rules/health.h — хиты, временные хиты и спасброски от смерти
// (FR-E113, DOC-01 FR-K20/FR-K21, таблица 8.6 примеры 7 и 13; п. Г.3 ТЗ DOC-04).
// Жёсткие правила:
//   1) урон списывается сначала с temp_hp, затем с hp_current (п. 4.1.11.4);
//   2) temp_hp не складываются — берётся максимум источника (AC-K04);
//   3) лечение temp_hp не восстанавливает;
//   4) 3 успеха — стабилизация, 3 провала — смерть.
// ВАЖНО (E-D01): файл не включает заголовки engine/*.
#pragma once

#include <cstdint>

namespace rules {

struct HealthState {
    int hp_max = 0;
    int hp_current = 0;
    int temp_hp = 0;
    int death_saves_success = 0;
    int death_saves_failure = 0;
};

// Порог спасбросков от смерти (таблица 8.6, пример 13)
inline constexpr int kDeathSaveThreshold = 3;

enum class DeathSaveOutcome : std::uint8_t { Alive, Unconscious, Stabilized, Dead };

// Хиты на 1-м уровне: max(HitDie) + Mod(CON) (формула 3 п. Г.13)
inline int hp_at_first_level(int hit_die_max, int con_modifier) {
    int hp = hit_die_max + con_modifier;
    return hp < 1 ? 1 : hp; // минимум 1 хит
}

// Хиты при повышении уровня: бросок или среднее значение кости + Mod(CON) (формула 4)
inline int hp_on_level_up(int hit_die_value, int con_modifier) {
    int gain = hit_die_value + con_modifier;
    return gain < 1 ? 1 : gain;
}

// Создание персонажа: текущие хиты равны максимальным
inline HealthState make_health(int hp_max) {
    HealthState h;
    h.hp_max = hp_max;
    h.hp_current = hp_max;
    return h;
}

// Временные хиты не складываются: при новом источнике выбирается максимум (AC-K04)
inline void grant_temp_hp(HealthState& h, int amount) {
    if (amount <= 0) return;
    if (amount > h.temp_hp) h.temp_hp = amount;
}

// Возвращает фактически нанесённый урон по hp_current (для журнала и событий)
inline int apply_damage(HealthState& h, int damage) {
    if (damage <= 0) return 0;
    const int to_temp = damage < h.temp_hp ? damage : h.temp_hp;
    h.temp_hp -= to_temp;
    const int to_hp = damage - to_temp;
    const int before = h.hp_current;
    h.hp_current -= to_hp;
    if (h.hp_current < 0) h.hp_current = 0;
    return before - h.hp_current;
}

// Лечение: temp_hp не восстанавливает; hp_current не превышает hp_max
inline int heal(HealthState& h, int amount) {
    if (amount <= 0) return 0;
    const int before = h.hp_current;
    h.hp_current += amount;
    if (h.hp_current > h.hp_max) h.hp_current = h.hp_max;
    return h.hp_current - before;
}

inline bool is_unconscious(const HealthState& h) { return h.hp_current <= 0; }

// Спасбросок от смерти: натуральный 20 возвращает 1 хит (правило 5e, SRD 5.1)
inline void record_death_save(HealthState& h, bool success, bool natural_20 = false) {
    if (natural_20) {
        h.death_saves_success = 0;
        h.death_saves_failure = 0;
        h.hp_current = 1;
        return;
    }
    if (success) {
        if (h.death_saves_success < kDeathSaveThreshold) ++h.death_saves_success;
    } else {
        if (h.death_saves_failure < kDeathSaveThreshold) ++h.death_saves_failure;
    }
}

inline void reset_death_saves(HealthState& h) {
    h.death_saves_success = 0;
    h.death_saves_failure = 0;
}

inline DeathSaveOutcome death_save_outcome(const HealthState& h) {
    if (h.hp_current > 0) return DeathSaveOutcome::Alive;
    if (h.death_saves_failure >= kDeathSaveThreshold) return DeathSaveOutcome::Dead;
    if (h.death_saves_success >= kDeathSaveThreshold) return DeathSaveOutcome::Stabilized;
    return DeathSaveOutcome::Unconscious;
}

} // namespace rules
