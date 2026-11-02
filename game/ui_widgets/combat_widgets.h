// game/ui_widgets/combat_widgets.h — боевые виджеты интерфейса (состав п. 4.1.8.3):
// InitiativeTracker, TurnResourceBar, ConditionIcons, CombatLog, CharacterSheetPanel,
// CreditsScreen. Виджеты принимают ТОЛЬКО DTO (E-D12, UI-E02): обращения к модулю
// правил из слоя UI запрещены (UI-E01, NFR-E10).
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "rules/dto.h"
#include "ui/layout.h"
#include "ui/widgets.h"

namespace game {

// --- Очередь инициативы (InitiativeTracker, FR-E86-подобный шаблон) -----------
class InitiativeTracker : public engine::ui::Panel {
public:
    InitiativeTracker();
    void set_tracker(const rules::InitiativeTrackerDTO& dto);
    const rules::InitiativeTrackerDTO& tracker() const { return dto_; }
    const std::vector<std::string>& rows() const { return rows_; }
    std::size_t row_count() const { return rows_.size(); }

    // Имя активного участника (для журнала и панели ресурсов хода)
    std::string active_name() const;

private:
    void rebuild();
    rules::InitiativeTrackerDTO dto_;
    std::vector<std::string> rows_;
};

// --- Панель ресурсов хода (TurnResourceBar, FR-E86) ---------------------------
class TurnResourceBar : public engine::ui::Panel {
public:
    TurnResourceBar();
    void set_resources(const rules::TurnResourceBarDTO& dto);
    const rules::TurnResourceBarDTO& resources() const { return dto_; }
    const std::vector<std::string>& rows() const { return rows_; }

private:
    void rebuild();
    rules::TurnResourceBarDTO dto_;
    std::vector<std::string> rows_;
};

// --- Иконки состояний (ConditionIcons, UI-E04) --------------------------------
class ConditionIcons : public engine::ui::Panel {
public:
    ConditionIcons();
    void set_conditions(std::vector<rules::ConditionDTO> conditions);
    const std::vector<rules::ConditionDTO>& conditions() const { return conditions_; }
    std::size_t icon_count() const { return conditions_.size(); }

    // Иконка — символ и подпись, а не только цвет (UI-E04)
    static std::string icon_glyph(const std::string& icon_key);

private:
    std::vector<rules::ConditionDTO> conditions_;
};

// --- Журнал событий боя (CombatLog, FR-E87: прокрутка и фильтрация) -----------
enum class CombatLogFilter : std::uint8_t {
    All, Rolls, Damage, Conditions, Turn, System
};

class CombatLog : public engine::ui::Panel {
public:
    CombatLog();

    void append(CombatLogFilter category, std::string text);
    void clear();

    void set_filter(CombatLogFilter filter);
    CombatLogFilter filter() const { return filter_; }

    // Видимые записи с учётом фильтра (FR-E87)
    std::vector<std::string> visible_entries() const;
    std::size_t entry_count() const { return entries_.size(); }
    std::size_t visible_count() const { return visible_entries().size(); }

    void scroll_to_latest();
    float scroll_offset() const { return scroll_->scroll_offset(); }

    engine::ui::ScrollView& scroll_view() { return *scroll_; }

private:
    struct Entry {
        CombatLogFilter category = CombatLogFilter::System;
        std::string text;
    };
    void rebuild();

    std::vector<Entry> entries_;
    CombatLogFilter filter_ = CombatLogFilter::All;
    std::shared_ptr<engine::ui::ScrollView> scroll_;
    std::size_t max_entries_ = 500; // защита от роста памяти в долгой сессии (NFR-E05)
};

// --- Карточка персонажа (CharacterSheetPanel) ---------------------------------
class CharacterSheetPanel : public engine::ui::Panel {
public:
    CharacterSheetPanel();
    void set_summary(const rules::CharacterSummaryDTO& dto);
    const rules::CharacterSummaryDTO& summary() const { return dto_; }
    const std::vector<std::string>& rows() const { return rows_; }

private:
    void rebuild();
    rules::CharacterSummaryDTO dto_;
    std::vector<std::string> rows_;
};

// --- Экран атрибуции ассетов (CreditsScreen, UI-E05, п. 4.6.4) ----------------
struct CreditEntry {
    std::string asset_name;
    std::string author;
    std::string source;
    std::string license;
};

class CreditsScreen : public engine::ui::Modal {
public:
    CreditsScreen();
    void set_credits(std::vector<CreditEntry> credits);
    const std::vector<CreditEntry>& credits() const { return credits_; }
    std::vector<std::string> rows() const;

private:
    void rebuild();
    std::vector<CreditEntry> credits_;
};

} // namespace game
