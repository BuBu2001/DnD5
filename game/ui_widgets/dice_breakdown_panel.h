// game/ui_widgets/dice_breakdown_panel.h — панель разбора броска (FR-E86, UI-E02, UI-E03).
// Шаблонный UI-компонент, принимающий DTO: слой UI не знает правил D&D (E-D02),
// преобразование rules::RollResult → RollBreakdownDTO выполняет адаптер игрового слоя.
// Требование UI-E03: отображаются ВСЕ слагаемые с их источниками и значениями.
// Требование UI-E04: преимущество и помеха различимы не только цветом —
// у каждого признака своя иконка (символ), подпись и форма маркера.
#pragma once

#include <string>
#include <vector>

#include "rules/dto.h"
#include "ui/layout.h"
#include "ui/widgets.h"

namespace game {

class DiceBreakdownPanel : public engine::ui::Panel {
public:
    DiceBreakdownPanel();

    // Приём DTO (единственный способ наполнить панель — FR-E86)
    void set_breakdown(const rules::RollBreakdownDTO& dto);
    void clear();

    const rules::RollBreakdownDTO& breakdown() const { return dto_; }

    // Строковое представление строки разбора: «+5 Бонус мастерства (Proficiency)»
    static std::string format_modifier(const rules::ModifierDTO& m);

    // Подписи признаков преимущества/помехи (UI-E04: не только цвет)
    static std::string advantage_marker(bool advantage, bool disadvantage, bool cancelled);

    // Число отображаемых слагаемых — для проверки полноты разбора (UI-E03)
    std::size_t modifier_row_count() const { return rows_.size(); }
    const std::vector<std::string>& rows() const { return rows_; }

private:
    void rebuild();

    rules::RollBreakdownDTO dto_;
    std::vector<std::string> rows_;
    engine::ui::Label title_;
};

} // namespace game
