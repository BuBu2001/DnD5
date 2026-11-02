// rules/formulas.h — перечень формул, переносимых из DOC-01 (п. Г.13).
// Каждая формула — отдельная функция и отдельный тест.
// ВНИМАНИЕ (E-D01): в каталоге rules/ запрещены включения SDL.h, GL/gl.h, engine/*.
#pragma once

#include <cmath>

namespace rules {

inline int ability_modifier(int score) { return static_cast<int>(std::floor((score - 10) / 2.0)); }
inline int proficiency_bonus(int level) { return (level - 1) / 4 + 2; }
inline int spell_save_dc(int mod, int proficiency) { return 8 + mod + proficiency; }
inline double success_probability(int dc, int mod) { return (21 - dc + mod) / 20.0; }
inline double advantage_probability(double p) { return 1.0 - (1.0 - p) * (1.0 - p); }
inline int passive_check(int mod, int proficiency) { return 10 + mod + proficiency; }
inline int resistant_damage(int damage) { return static_cast<int>(std::floor(damage / 2.0)); }
inline int vulnerable_damage(int damage) { return damage * 2; }
inline int feet_to_cells(int feet) { return feet / 5; }

} // namespace rules
