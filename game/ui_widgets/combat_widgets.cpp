// game/ui_widgets/combat_widgets.cpp — реализация боевых виджетов (п. 4.1.8.3).
#include "ui_widgets/combat_widgets.h"

#include <algorithm>

namespace game {

namespace {

constexpr float kRowHeight = 20.f;

std::shared_ptr<engine::ui::VerticalBox> make_rows(const std::vector<std::string>& rows,
                                                   float x, float y) {
    auto box = std::make_shared<engine::ui::VerticalBox>();
    box->set_position(engine::ui::Vec2{x, y});
    box->set_spacing(2.f);
    for (const auto& text : rows) {
        auto label = std::make_shared<engine::ui::Label>(text);
        label->set_size(engine::ui::Size{300.f, kRowHeight});
        box->add_child(label);
    }
    return box;
}

} // namespace

// --- InitiativeTracker -------------------------------------------------------
InitiativeTracker::InitiativeTracker() {
    set_name("InitiativeTracker");
    set_size(engine::ui::Size{220.f, 320.f});
    set_padding(engine::ui::Insets{8.f, 8.f, 8.f, 8.f});
}

void InitiativeTracker::set_tracker(const rules::InitiativeTrackerDTO& dto) {
    dto_ = dto;
    rebuild();
}

std::string InitiativeTracker::active_name() const {
    if (dto_.active_index >= dto_.entries.size()) return {};
    return dto_.entries[dto_.active_index].name;
}

void InitiativeTracker::rebuild() {
    clear_children();
    rows_.clear();
    rows_.push_back("Раунд " + std::to_string(dto_.round));
    for (std::size_t i = 0; i < dto_.entries.size(); ++i) {
        const auto& e = dto_.entries[i];
        const std::string marker = (i == dto_.active_index) ? ">" : " ";
        rows_.push_back(marker + " " + std::to_string(e.score) + "  " +
                        (e.name.empty() ? ("entity#" + std::to_string(e.entity)) : e.name));
    }
    add_child(make_rows(rows_, padding().left, padding().top));
}

// --- TurnResourceBar ---------------------------------------------------------
TurnResourceBar::TurnResourceBar() {
    set_name("TurnResourceBar");
    set_size(engine::ui::Size{320.f, 96.f});
    set_padding(engine::ui::Insets{8.f, 8.f, 8.f, 8.f});
}

void TurnResourceBar::set_resources(const rules::TurnResourceBarDTO& dto) {
    dto_ = dto;
    rebuild();
}

void TurnResourceBar::rebuild() {
    clear_children();
    rows_.clear();
    const auto mark = [](bool available) { return available ? "[X]" : "[ ]"; };
    rows_.push_back(dto_.entity_name.empty() ? std::string("Ресурсы хода") : dto_.entity_name);
    rows_.push_back(std::string(mark(dto_.action_available)) + " Действие   " +
                    std::string(mark(dto_.bonus_action_available)) + " Бонусное");
    rows_.push_back(std::string(mark(dto_.reaction_available)) + " Реакция    " +
                    std::string(mark(dto_.free_interaction_available)) + " Взаимодействие");
    rows_.push_back("Перемещение: " + std::to_string(dto_.movement_remaining_feet) + " футов (" +
                    std::to_string(dto_.movement_remaining_cells) + " клеток)");
    add_child(make_rows(rows_, padding().left, padding().top));
}

// --- ConditionIcons ----------------------------------------------------------
ConditionIcons::ConditionIcons() {
    set_name("ConditionIcons");
    set_size(engine::ui::Size{220.f, 40.f});
    set_padding(engine::ui::Insets{4.f, 4.f, 4.f, 4.f});
}

std::string ConditionIcons::icon_glyph(const std::string& icon_key) {
    // UI-E04: признак различим формой/подписью, а не только цветом.
    // До подключения шрифтового атласа (M2) иконки — символьные маркеры.
    if (icon_key == "icon_restrained") return "{R}";
    if (icon_key == "icon_blinded") return "{B}";
    if (icon_key == "icon_prone") return "{P}";
    if (icon_key == "icon_grappled") return "{G}";
    if (icon_key == "icon_stunned") return "{S}";
    if (icon_key == "icon_invisible") return "{I}";
    return "{?}";
}

void ConditionIcons::set_conditions(std::vector<rules::ConditionDTO> conditions) {
    conditions_ = std::move(conditions);
    clear_children();
    std::vector<std::string> rows;
    for (const auto& c : conditions_) {
        rows.push_back(icon_glyph(c.icon_key) + " " + c.name);
    }
    if (rows.empty()) rows.push_back("Состояний нет");
    add_child(make_rows(rows, padding().left, padding().top));
    set_size(engine::ui::Size{size().w,
                              padding().top + padding().bottom +
                                  static_cast<float>(rows.size()) * kRowHeight});
}

// --- CombatLog ---------------------------------------------------------------
CombatLog::CombatLog() {
    set_name("CombatLog");
    set_size(engine::ui::Size{420.f, 240.f});
    set_padding(engine::ui::Insets{6.f, 6.f, 6.f, 6.f});
    scroll_ = std::make_shared<engine::ui::ScrollView>();
    scroll_->set_name("CombatLogScroll");
    scroll_->set_position(engine::ui::Vec2{padding().left, padding().top});
    scroll_->set_size(engine::ui::Size{size().w - padding().left - padding().right,
                                       size().h - padding().top - padding().bottom});
    add_child(scroll_);
}

void CombatLog::append(CombatLogFilter category, std::string text) {
    entries_.push_back(Entry{category, std::move(text)});
    // Ограничение числа записей: память не растёт в долгой сессии (NFR-E05)
    if (entries_.size() > max_entries_) {
        const std::ptrdiff_t excess =
            static_cast<std::ptrdiff_t>(entries_.size() - max_entries_);
        entries_.erase(entries_.begin(), entries_.begin() + excess);
    }
    rebuild();
    scroll_to_latest();
}

void CombatLog::clear() {
    entries_.clear();
    rebuild();
}

void CombatLog::set_filter(CombatLogFilter filter) {
    if (filter_ == filter) return;
    filter_ = filter;
    rebuild();
}

std::vector<std::string> CombatLog::visible_entries() const {
    std::vector<std::string> out;
    for (const auto& e : entries_) {
        if (filter_ == CombatLogFilter::All || e.category == filter_) out.push_back(e.text);
    }
    return out;
}

void CombatLog::scroll_to_latest() { scroll_->scroll_to_bottom(); }

void CombatLog::rebuild() {
    const auto rows = visible_entries();
    scroll_->clear_children();
    scroll_->add_child(make_rows(rows, 0.f, 0.f));
    scroll_->set_content_size(engine::ui::Size{scroll_->size().w,
                                               static_cast<float>(rows.size()) * kRowHeight});
}

// --- CharacterSheetPanel -----------------------------------------------------
CharacterSheetPanel::CharacterSheetPanel() {
    set_name("CharacterSheetPanel");
    set_size(engine::ui::Size{260.f, 220.f});
    set_padding(engine::ui::Insets{8.f, 8.f, 8.f, 8.f});
}

void CharacterSheetPanel::set_summary(const rules::CharacterSummaryDTO& dto) {
    dto_ = dto;
    rebuild();
}

void CharacterSheetPanel::rebuild() {
    clear_children();
    rows_.clear();
    rows_.push_back(dto_.name + "  (уровень " + std::to_string(dto_.level) + ")");
    rows_.push_back("AC " + std::to_string(dto_.ac) + "   Скорость " +
                    std::to_string(dto_.speed_feet) + " фут.");
    rows_.push_back("HP " + std::to_string(dto_.hp_current) + "/" +
                    std::to_string(dto_.hp_max) +
                    (dto_.temp_hp > 0 ? "  (+" + std::to_string(dto_.temp_hp) + " временные)" : ""));
    if (!dto_.conditions.empty()) {
        std::string line = "Состояния:";
        for (const auto& c : dto_.conditions) line += " " + c.name;
        rows_.push_back(line);
    }
    add_child(make_rows(rows_, padding().left, padding().top));
}

// --- CreditsScreen -----------------------------------------------------------
CreditsScreen::CreditsScreen() {
    set_name("CreditsScreen");
    set_size(engine::ui::Size{720.f, 480.f});
    set_position(engine::ui::Vec2{280.f, 120.f});
    set_padding(engine::ui::Insets{12.f, 12.f, 12.f, 12.f});
}

void CreditsScreen::set_credits(std::vector<CreditEntry> credits) {
    credits_ = std::move(credits);
    rebuild();
}

std::vector<std::string> CreditsScreen::rows() const {
    // Состав атрибуции по п. 4.6.4: наименование ассета, автор, источник, лицензия
    std::vector<std::string> out;
    out.push_back("Атрибуция ассетов (DOC-03)");
    for (const auto& c : credits_) {
        out.push_back(c.asset_name + " — " + c.author + " — " + c.source + " — " + c.license);
    }
    if (credits_.empty()) out.push_back("(список ассетов не загружен)");
    return out;
}

void CreditsScreen::rebuild() {
    clear_children();
    add_child(make_rows(rows(), padding().left, padding().top));
}

} // namespace game
