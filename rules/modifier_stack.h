// rules/modifier_stack.h — слои модификаторов (п. 4.1.11.5, п. Г.7).
// Смешивание слоёв в одной формуле запрещено; суммирование — на последнем шаге.
#pragma once

#include <string>
#include <unordered_map>
#include <vector>

namespace rules {

enum class ModifierSource {
    Base, Ability, Proficiency, Feature, Item,
    Condition, Spell, TemporaryBuff, TemporaryDebuff
};

struct Modifier {
    int value = 0;
    ModifierSource source = ModifierSource::Base;
    std::string source_id;
    bool stacks = false; // одноимённые не-stacking эффекты: применяется сильнейший
};

inline int aggregate(const std::vector<Modifier>& mods) {
    // группировка по слоям; для не-stacking источников с одним source_id — максимум
    std::unordered_map<std::string, int> best_per_source;
    int total = 0;
    for (const auto& m : mods) {
        if (m.stacks || m.source_id.empty()) {
            total += m.value;
        } else {
            auto it = best_per_source.find(m.source_id);
            if (it == best_per_source.end()) best_per_source[m.source_id] = m.value;
            else if (m.value > it->second) it->second = m.value;
        }
    }
    for (const auto& [id, v] : best_per_source) total += v;
    return total;
}

} // namespace rules
