// game/ui_widgets/dice_breakdown_panel.cpp — реализация панели разбора броска (UI-E03).
#include "ui_widgets/dice_breakdown_panel.h"

namespace game {

namespace {

const char* modifier_source_name(rules::ModifierSource source) {
    switch (source) {
        case rules::ModifierSource::Base:            return "Base";
        case rules::ModifierSource::Ability:         return "Ability";
        case rules::ModifierSource::Proficiency:     return "Proficiency";
        case rules::ModifierSource::Feature:         return "Feature";
        case rules::ModifierSource::Item:            return "Item";
        case rules::ModifierSource::Condition:       return "Condition";
        case rules::ModifierSource::Spell:           return "Spell";
        case rules::ModifierSource::TemporaryBuff:   return "TemporaryBuff";
        case rules::ModifierSource::TemporaryDebuff: return "TemporaryDebuff";
    }
    return "Unknown";
}

} // namespace

DiceBreakdownPanel::DiceBreakdownPanel() {
    set_name("DiceBreakdownPanel");
    set_size(engine::ui::Size{360.f, 220.f});
    set_padding(engine::ui::Insets{8.f, 8.f, 8.f, 8.f});
}

void DiceBreakdownPanel::set_breakdown(const rules::RollBreakdownDTO& dto) {
    dto_ = dto;
    rebuild();
}

void DiceBreakdownPanel::clear() {
    dto_ = rules::RollBreakdownDTO{};
    rows_.clear();
    clear_children();
}

std::string DiceBreakdownPanel::format_modifier(const rules::ModifierDTO& m) {
    const std::string sign = m.value >= 0 ? "+" : "";
    return sign + std::to_string(m.value) + "  " + m.label + "  (" +
           modifier_source_name(m.source) + (m.source_id.empty() ? "" : ": " + m.source_id) + ")";
}

std::string DiceBreakdownPanel::advantage_marker(bool advantage, bool disadvantage,
                                                 bool cancelled) {
    // UI-E04: признак различим подписью и символом-иконкой, а не только цветом
    if (cancelled) return "[=] Преимущество и помеха аннулированы — один d20";
    if (advantage && !disadvantage) return "[^] Преимущество: выбран больший d20";
    if (disadvantage && !advantage) return "[v] Помеха: выбран меньший d20";
    return "[ ] Обычный бросок";
}

void DiceBreakdownPanel::rebuild() {
    clear_children();
    rows_.clear();

    title_.set_text(dto_.title.empty() ? std::string("Бросок") : dto_.title);
    title_.set_font_size(style().font_size + 2.f);
    auto title = std::make_shared<engine::ui::Label>(title_.text());
    title->set_font_size(title_.font_size());
    add_child(title);

    // Значения кубиков: при преимуществе/помехе видны оба, выбранное помечено
    std::string dice_line = "d20:";
    for (std::size_t i = 0; i < dto_.dice.size(); ++i) {
        const bool chosen = dto_.dice.size() > 1 && dto_.dice[i] == dto_.chosen_die;
        const std::string value = (i < dto_.dice_values.size() && !dto_.dice_values[i].empty())
                                      ? dto_.dice_values[i]
                                      : std::to_string(dto_.dice[i]);
        dice_line += " ";
        dice_line += chosen ? "[" + value + "]" : value;
    }
    rows_.push_back(dice_line);
    rows_.push_back(advantage_marker(dto_.advantage, dto_.disadvantage, dto_.cancelled));

    // Все слагаемые с источниками и значениями (UI-E03)
    for (const auto& m : dto_.modifiers) {
        rows_.push_back(format_modifier(m));
    }

    rows_.push_back("Итого: " + std::to_string(dto_.total) + "  против DC/AC " +
                    std::to_string(dto_.dc) + "  →  " + (dto_.success ? "УСПЕХ" : "ПРОВАЛ"));

    auto box = std::make_shared<engine::ui::VerticalBox>();
    box->set_position(engine::ui::Vec2{padding().left, padding().top + 20.f});
    for (const auto& row : rows_) {
        auto label = std::make_shared<engine::ui::Label>(row);
        label->set_font_size(style().font_size);
        box->add_child(label);
    }
    add_child(box);

    // Высота панели подстраивается под число строк (retained-mode, FR-E80)
    const float row_height = style().font_size + 6.f;
    set_size(engine::ui::Size{size().w,
                              padding().top + padding().bottom + 24.f +
                                  static_cast<float>(rows_.size()) * row_height});
}

} // namespace game
