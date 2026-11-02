// rules/proficiency.h — бонус мастерства и владение (FR-E111, DOC-01 FR-K24, п. Г.2).
// Формула: ProficiencyBonus = floor((Level − 1) / 4) + 2 (формула 2 п. Г.13).
// Мультикласс считается по суммарному уровню (AC-K02: Воин 3 / Плут 2 → уровень 5 → +3).
// Типы Half и Double заложены сразу, но в MVP применяются только None и Full (п. Г.2).
// ВАЖНО (E-D01): файл не включает заголовки engine/*.
#pragma once

#include <cstdint>
#include <algorithm>
#include <cstddef>
#include <initializer_list>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace rules {

enum class ProficiencyType : std::uint8_t { None, Half, Full, Double };

struct ProficiencyBonus {
    std::int8_t level = 1;
    std::int8_t bonus = 2;
};

// Бонус мастерства по общему уровню (1..20)
inline int proficiency_bonus_for_level(int level) {
    if (level < 1) level = 1;
    return (level - 1) / 4 + 2;
}

// Мультикласс: суммарный уровень всех классов (AC-K02)
inline int total_character_level(std::initializer_list<int> class_levels) {
    int total = 0;
    for (const int l : class_levels) total += (l > 0 ? l : 0);
    return total;
}

inline ProficiencyBonus make_proficiency(int total_level) {
    ProficiencyBonus pb;
    pb.level = static_cast<std::int8_t>(total_level < 1 ? 1 : total_level);
    pb.bonus = static_cast<std::int8_t>(proficiency_bonus_for_level(pb.level));
    return pb;
}

// Вклад владения в бросок с учётом типа (Half/Double — V1, п. Г.2)
inline int proficiency_contribution(int full_bonus, ProficiencyType type) {
    switch (type) {
        case ProficiencyType::None:   return 0;
        case ProficiencyType::Half:   return full_bonus / 2;  // floor, округление вниз
        case ProficiencyType::Full:   return full_bonus;
        case ProficiencyType::Double: return full_bonus * 2;
    }
    return 0;
}

// Флаги владения: навыки, оружие, спасброски (DOC-01 FR-K24, DATA-K11).
// Идентификаторы владения — стабильные строки из данных (DATA-E03),
// например "Skill_Stealth", "Weapon_Longsword", "Save_Dex".
class ProficiencySet {
public:
    void grant(ProficiencyType type, std::string id) {
        if (id.empty()) return;
        auto [it, inserted] = entries_.try_emplace(std::move(id), type);
        // При повторной выдаче оставляем сильнейшее владение (одноимённые — в одном
        // экземпляре, п. 4.1.11.5).
        if (!inserted && type > it->second) it->second = type;
    }

    void revoke(const std::string& id) { entries_.erase(id); }

    ProficiencyType type_of(const std::string& id) const {
        const auto it = entries_.find(id);
        return it == entries_.end() ? ProficiencyType::None : it->second;
    }

    bool has(const std::string& id) const { return type_of(id) != ProficiencyType::None; }

    // Бонус мастерства добавляется к одному броску не более одного раза (FR-K35):
    // вызывающий код запрашивает вклад ровно один раз на бросок.
    int bonus_for(const std::string& id, int full_bonus) const {
        return proficiency_contribution(full_bonus, type_of(id));
    }

    std::size_t size() const { return entries_.size(); }

    // Отсортированный перечень идентификаторов — для сохранений и UI (детерминизм, NFR-E09)
    std::vector<std::string> ids() const {
        std::vector<std::string> out;
        out.reserve(entries_.size());
        for (const auto& [id, type] : entries_) out.push_back(id);
        std::sort(out.begin(), out.end());
        return out;
    }

private:
    std::unordered_map<std::string, ProficiencyType> entries_;
};

} // namespace rules
