// rules/features.cpp — резолверы особых умений Категории 1 (FR-E121, DOC-01 FR-K90).
#include "rules/features.h"

#include <algorithm>

namespace rules {

FeatureEffect resolve_sneak_attack(const FeatureContext& ctx) {
    FeatureEffect out;
    out.feature_id = "Feature_SneakAttack";
    out.damage_type = DamageType::Piercing; // базовый тип; уточняется оружием в игре

    const bool qualified = ctx.has_advantage || ctx.ally_adjacent_to_target;
    if (!qualified) return out; // applies = false

    out.applies = true;
    out.extra_dice_count = ctx.class_level < 1 ? 1 : ctx.class_level; // 1d6 × уровень Плута
    out.extra_dice_sides = 6;
    out.reason = ctx.has_advantage ? "преимущество на атаку"
                                   : "союзник рядом с целью";
    return out;
}

FeatureEffect resolve_rage_damage_bonus(const FeatureContext& ctx) {
    FeatureEffect out;
    out.feature_id = "Feature_Rage";
    out.damage_type = DamageType::Bludgeoning;
    // В MVP (Категория 1, упрощение): ярость даёт +2 к урону ближнего боя.
    // Полный RAW (сопротивления, число использований, концентрация) — версия 1.x,
    // см. таблицу 4.13 ТЗ DOC-04 (Rage вне скоупа MVP в полной редакции).
    if (!ctx.melee) return out;
    out.applies = true;
    out.flat_bonus = 2; // плоский бонус к урону, без дополнительных костей
    out.reason = "ярость: +2 к урону ближнего боя";
    return out;
}

} // namespace rules
