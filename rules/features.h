// rules/features.h — особые умения как резолверы (FR-E121, DOC-01 FR-K90…FR-K93).
// Категории сложности (FR-K90…FR-K92): в MVP реализуются только умения Категории 1
// (линейные механики, минимум исключений). Категория 2 — версия 1.x, Категория 3 — вне скоупа.
// Умение не хардкодится в боевом коде: резолвер получает контекст и возвращает
// дополнительные кости урона и/или модификаторы (E-D09, NFR-E13).
// ВАЖНО (E-D01): файл не включает заголовки engine/*.
#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "rules/damage.h"

namespace rules {

enum class FeatureCategory : std::uint8_t {
    Category1, // линейные механики (MVP, FR-K90)
    Category2, // слоты заклинаний / уникальные пулы (версия 1.x, FR-K91)
    Category3  // ситуативные правила (вне скоупа MVP, FR-K92)
};

// Контекст применения умения: данные, а не ссылки на внутренние структуры (E-D12)
struct FeatureContext {
    bool has_advantage = false;        // преимущество на атаку
    bool melee = true;                 // ближний бой
    bool ally_adjacent_to_target = false; // союзник рядом с целью (условие Sneak Attack)
    int class_level = 1;               // уровень класса, дающего умение
    bool target_prone_or_restrained = false;
};

// Результат резолвера: дополнительные кости урона и признаки для журнала/UI
struct FeatureEffect {
    bool applies = false;
    std::string feature_id;            // "Feature_SneakAttack"
    std::string reason;                // человекочитаемое обоснование (UI-E03)
    int extra_dice_count = 0;          // число дополнительных костей
    int extra_dice_sides = 0;          // граней у каждой кости
    int flat_bonus = 0;                // плоский бонус к урону (без броска)
    DamageType damage_type = DamageType::Piercing;
};

// Sneak Attack (Плут, Категория 1, формула 18 п. Г.13):
// 1d6 × уровень Плута при преимуществе ИЛИ союзнике рядом с целью.
// Умение применяется не чаще одного раза за ход (проверяет вызывающий код).
FeatureEffect resolve_sneak_attack(const FeatureContext& ctx);

// Rage (Варвар, Категория 1 в упрощении MVP): бонус к урону ближнего боя,
// сопротивление дробящему/колющему/рубящему. Полный RAW-вариант — V1 (таблица 4.13).
FeatureEffect resolve_rage_damage_bonus(const FeatureContext& ctx);

// Реестр умений персонажа: какие резолверы вызывать (данные из JSON, NFR-E13)
class FeatureSet {
public:
    void add(std::string feature_id, FeatureCategory category) {
        features_.push_back(Feature{std::move(feature_id), category});
    }

    bool has(const std::string& feature_id) const {
        return std::any_of(features_.begin(), features_.end(),
                           [&](const Feature& f) { return f.id == feature_id; });
    }

    // Умения Категории 1 — только они применяются в MVP (FR-K93, AC-K21)
    std::vector<std::string> category1_ids() const {
        std::vector<std::string> out;
        for (const auto& f : features_) {
            if (f.category == FeatureCategory::Category1) out.push_back(f.id);
        }
        std::sort(out.begin(), out.end());
        return out;
    }

    std::size_t size() const { return features_.size(); }

private:
    struct Feature {
        std::string id;
        FeatureCategory category = FeatureCategory::Category1;
    };
    std::vector<Feature> features_;
};

} // namespace rules
