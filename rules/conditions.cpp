// rules/conditions.cpp — эталонная таблица шести обязательных состояний и резолвер
// (FR-E116, DOC-01 таблица 4.9; E-D08 — декларативные флаги вместо ветвлений).
#include "rules/conditions.h"

#include <algorithm>

namespace rules {

namespace {

ConditionDef make_def(ConditionId id, std::string name, std::string icon, ConditionFlags flags,
                      std::string description) {
    ConditionDef def;
    def.id = std::move(id);
    def.name = std::move(name);
    def.icon_key = std::move(icon);
    def.flags = flags;
    def.description = std::move(description);
    return def;
}

} // namespace

ConditionTable make_standard_conditions() {
    ConditionTable table;

    // Опутан (Restrained): скорость 0; помеха на свои атаки и проверки/спасброски DEX;
    // атакующие получают преимущество.
    table.add(make_def("Condition_Restrained", "Опутан", "icon_restrained",
                       ConditionFlags::SpeedZero | ConditionFlags::AttackDisadvantageBy |
                           ConditionFlags::SavingThrowDisadvantageDex |
                           ConditionFlags::AttackAdvantageAgainst,
                       "Скорость 0; помеха на броски атаки и спасброски Ловкости; "
                       "атакующие бьют с преимуществом."));

    // Ослеплён (Blinded): скорость без изменений; автопровал проверок, требующих зрения;
    // помеха на свои атаки; атакующие — преимущество.
    table.add(make_def("Condition_Blinded", "Ослеплён", "icon_blinded",
                       ConditionFlags::AutoFailVisionChecks | ConditionFlags::AttackDisadvantageBy |
                           ConditionFlags::AttackAdvantageAgainst,
                       "Автоматический провал проверок, требующих зрения; помеха на свои атаки; "
                       "атакующие бьют с преимуществом."));

    // Лежит ничком (Prone): скорость 0 (ползание +1 фут за фут); помеха на свои атаки;
    // ближний бой по существу — преимущество, дальний — помеха; встать — половина скорости.
    table.add(make_def("Condition_Prone", "Лежит ничком", "icon_prone",
                       ConditionFlags::SpeedZero | ConditionFlags::AttackDisadvantageBy |
                           ConditionFlags::MeleeAttackAdvantage |
                           ConditionFlags::RangedAttackDisadvantageAgainst |
                           ConditionFlags::StandUpCostsHalfSpeed | ConditionFlags::CrawlCostsExtra,
                       "Скорость 0 (ползание — двойной расход); помеха на свои атаки; "
                       "в ближнем бою по существу бьют с преимуществом, из дальней атаки — с помехой; "
                       "подняться стоит половину скорости."));

    // Схвачен (Grappled): скорость 0; спадает, если схвативший отпустил или оттолкнут.
    table.add(make_def("Condition_Grappled", "Схвачен", "icon_grappled",
                       ConditionFlags::SpeedZero | ConditionFlags::EndsWhenGrapplerReleases,
                       "Скорость 0. Состояние спадает, если схвативший отпустил цель или оттолкнут."));

    // Ошеломлён (Stunned): скорость 0; автопровал спасбросков Силы и Ловкости; не может
    // действовать; атакующие — преимущество; попадание в ближнем бою — критический удар
    // (авторское усиление, DOC-01 OQ-08).
    table.add(make_def("Condition_Stunned", "Ошеломлён", "icon_stunned",
                       ConditionFlags::SpeedZero | ConditionFlags::Incapacitated |
                           ConditionFlags::CannotTakeReactions | ConditionFlags::AutomaticFailureStrDex |
                           ConditionFlags::AttackAdvantageAgainst | ConditionFlags::CritOnMeleeHit,
                       "Скорость 0; автоматические провалы спасбросков Силы и Ловкости; "
                       "не может действовать; атакующие бьют с преимуществом; "
                       "любое попадание в ближнем бою — критический удар."));

    // Невидим (Invisible): скорость без изменений; преимущество на свои атаки;
    // атаки по существу — с помехой; атакующий обязан указать клетку.
    table.add(make_def("Condition_Invisible", "Невидим", "icon_invisible",
                       ConditionFlags::GrantsAttackAdvantage | ConditionFlags::AttackDisadvantageBy |
                           ConditionFlags::RequiresGuessedCell,
                       "Преимущество на свои атаки; атаки по существу совершаются с помехой; "
                       "атакующий обязан указать клетку — при ошибке автоматический промах."));

    return table;
}

ConditionSummary collect(const ConditionTable& table,
                        const std::vector<ConditionInstance>& active) {
    ConditionSummary out;
    for (const auto& inst : active) {
        const ConditionDef* def = table.find(inst.id);
        if (def == nullptr) continue; // неизвестное состояние игнорируется (NFR-E14)
        out.flags = out.flags | def->flags;
        for (const auto& m : def->modifiers) out.modifiers.push_back(m);
        if (def->speed_override_feet >= 0) {
            // несколько переопределений скорости: берём наименьшую (самое строгое)
            out.speed_override_feet = out.speed_override_feet < 0
                                          ? def->speed_override_feet
                                          : std::min(out.speed_override_feet, def->speed_override_feet);
        }
    }
    out.incapacitated = has_flag(out.flags, ConditionFlags::Incapacitated);
    out.cannot_take_reactions = has_flag(out.flags, ConditionFlags::CannotTakeReactions);
    if (has_flag(out.flags, ConditionFlags::SpeedZero)) out.speed_override_feet = 0;
    return out;
}

namespace {

void push_reason(std::vector<std::string>& reasons, const char* reason) {
    reasons.emplace_back(reason);
}

} // namespace

AdvantageResolution resolve_attacker_attack_modifiers(const ConditionSummary& attacker,
                                                     const AttackContext& ctx) {
    AdvantageResolution r;
    const auto f = attacker.flags;

    if (has_flag(f, ConditionFlags::GrantsAttackAdvantage)) {
        r.advantage = true;
        push_reason(r.reasons, "Condition_Invisible: преимущество на свои атаки");
    }
    if (has_flag(f, ConditionFlags::AttackDisadvantageBy)) {
        r.disadvantage = true;
        push_reason(r.reasons, "Состояние атакующего: помеха на броски атаки");
    }
    if (has_flag(f, ConditionFlags::AutoFailVisionChecks) && ctx.requires_sight) {
        r.disadvantage = true;
        push_reason(r.reasons, "Condition_Blinded: цель не видна");
    }
    if (ctx.melee) {
        if (has_flag(f, ConditionFlags::MeleeAttackDisadvantage)) {
            r.disadvantage = true;
            push_reason(r.reasons, "Condition_Prone: помеха на ближнюю атаку");
        }
    } else if (has_flag(f, ConditionFlags::RangedAttackDisadvantage)) {
        r.disadvantage = true;
        push_reason(r.reasons, "Condition_Prone: помеха на дальнюю атаку");
    }
    return r;
}

AdvantageResolution resolve_target_attack_modifiers(const ConditionSummary& target,
                                                   const AttackContext& ctx) {
    AdvantageResolution r;
    const auto f = target.flags;

    if (has_flag(f, ConditionFlags::AttackAdvantageAgainst)) {
        r.advantage = true;
        push_reason(r.reasons, "Состояние цели: атакующие бьют с преимуществом");
    }
    if (has_flag(f, ConditionFlags::RequiresGuessedCell)) {
        r.disadvantage = true;
        push_reason(r.reasons, "Condition_Invisible: атаки по существу с помехой");
    }
    if (ctx.melee && has_flag(f, ConditionFlags::MeleeAttackAdvantage)) {
        r.advantage = true;
        push_reason(r.reasons, "Condition_Prone: преимущество в ближнем бою");
    }
    if (!ctx.melee && has_flag(f, ConditionFlags::RangedAttackDisadvantageAgainst)) {
        r.disadvantage = true;
        push_reason(r.reasons, "Condition_Prone: помеха на дальнюю атаку");
    }
    return r;
}

CombinedAdvantage combine(const AdvantageResolution& attacker,
                          const AdvantageResolution& target) {
    CombinedAdvantage out;
    out.advantage = attacker.advantage || target.advantage;
    out.disadvantage = attacker.disadvantage || target.disadvantage;
    out.reasons = attacker.reasons;
    out.reasons.insert(out.reasons.end(), target.reasons.begin(), target.reasons.end());
    // Аннулирование (один d20, cancelled=true) выполняет DiceRoller — п. 4.1.11.2;
    // здесь оба флага сохраняются, чтобы UI показал причину (UI-E04).
    return out;
}

} // namespace rules
