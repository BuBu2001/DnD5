// engine/ui/control.h — базовый элемент retained-mode UI (FR-E80, FR-E84).
//
// ЖЁСТКОЕ ОГРАНИЧЕНИЕ (UI-E01, E-D02, NFR-E10, п. 4.8.2): в каталоге engine/ui/*
// запрещены включения rules/* — слой UI не знает понятий D&D (AC, спасбросок,
// Sneak Attack, ячейки заклинаний). UI получает ТОЛЬКО данные: числа, строки,
// флаги и DTO (E-D12). Игровые виджеты живут в game/ui_widgets/.
//
// Разметка задаётся в логических единицах базового разрешения 1280×720;
// пересчёт в физические пиксели выполняет слой отрисовки (FR-E02, FR-E84).
#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "render/renderer.h"

namespace engine::ui {

struct Vec2 { float x = 0.f, y = 0.f; };
struct Size { float w = 0.f, h = 0.f; };
struct Insets { float left = 0.f, top = 0.f, right = 0.f, bottom = 0.f; };

using ControlPtr = std::shared_ptr<class Control>;

// Типы событий ввода, которые обрабатывает UI-дерево (FR-E80)
enum class UiEventType : std::uint8_t {
    None, PointerMoved, PointerPressed, PointerReleased, PointerEnter, PointerLeave, Click
};

struct UiEvent {
    UiEventType type = UiEventType::None;
    Vec2 position{};     // в локальных координатах получателя
    Vec2 root_position{};// в логических координатах экрана 1280×720
    std::uint8_t button = 0;
    bool handled = false;
};

// Тема и стили: цвета, шрифты, отступы (FR-E85 — загрузка из файла данных, V1;
// значения по умолчанию задаются здесь, чтобы MVP не зависел от ассетов)
struct Style {
    render::Color background{0.10f, 0.11f, 0.14f, 1.0f};
    render::Color foreground{0.92f, 0.93f, 0.95f, 1.0f};
    render::Color accent{0.35f, 0.55f, 0.90f, 1.0f};
    render::Color danger{0.85f, 0.30f, 0.25f, 1.0f};
    render::Color success{0.35f, 0.75f, 0.40f, 1.0f};
    float padding = 6.f;
    float spacing = 4.f;
    float font_size = 16.f;
    std::string font_id = "assets:/fonts/default.ttf";
};

class Control : public std::enable_shared_from_this<Control> {
public:
    virtual ~Control() = default;

    // --- иерархия (FR-E80) ---------------------------------------------------
    void add_child(ControlPtr child);
    void remove_child(const ControlPtr& child);
    void clear_children();
    const std::vector<ControlPtr>& children() const { return children_; }
    Control* parent() const { return parent_; }
    std::size_t child_count() const { return children_.size(); }

    // --- геометрия -----------------------------------------------------------
    void set_position(Vec2 p) { position_ = p; }
    void set_size(Size s) { size_ = s; }
    Vec2 position() const { return position_; }
    Size size() const { return size_; }
    void set_padding(Insets p) { padding_ = p; }
    Insets padding() const { return padding_; }

    // Абсолютные координаты в логическом пространстве экрана (сумма смещений родителей)
    Vec2 absolute_position() const;
    render::Rect absolute_rect() const;

    // --- состояние -----------------------------------------------------------
    void set_visible(bool v) { visible_ = v; }
    bool visible() const { return visible_; }
    void set_enabled(bool e) { enabled_ = e; }
    bool enabled() const { return enabled_; }
    void set_name(std::string name) { name_ = std::move(name); }
    const std::string& name() const { return name_; }
    void set_style(const Style& style) { style_ = style; }
    const Style& style() const { return style_; }

    bool hovered() const { return hovered_; }
    bool pressed() const { return pressed_; }

    // Попадание точки (в логических координатах экрана) в элемент
    bool hit_test(Vec2 root_point) const;

    // Пересчёт экранных координат в локальные координаты элемента
    Vec2 to_local(Vec2 root_point) const {
        const Vec2 p = absolute_position();
        return Vec2{root_point.x - p.x, root_point.y - p.y};
    }

    // --- обход и отрисовка ---------------------------------------------------
    void update_all(double dt_logic);
    void draw_all(render::IRenderer& renderer);

    // Маршрутизация события от корня к потомкам; возвращает true, если событие
    // обработано (обработка прерывает обход — поведение модальных окон, FR-E83)
    bool dispatch_event(UiEvent& event);

    // Точка поиска элемента по имени (отладочная панель, FR-E100)
    Control* find_by_name(const std::string& name);

    std::size_t subtree_size() const;

protected:
    // Переопределяемые обработчики; базовая реализация ничего не делает
    virtual void on_update(double dt_logic) { (void)dt_logic; }
    virtual void on_draw(render::IRenderer& renderer) { (void)renderer; }
    virtual bool on_event(UiEvent& event) { (void)event; return false; }
    // Перекрывающие элементы (модальные окна, тултипы) не пропускают событие детям
    virtual bool blocks_children() const { return false; }

    Vec2 position_{};
    Size size_{};
    Insets padding_{};
    Style style_{};
    std::string name_;
    bool visible_ = true;
    bool enabled_ = true;
    bool hovered_ = false;
    bool pressed_ = false;

private:
    std::vector<ControlPtr> children_;
    Control* parent_ = nullptr;
};

// Фабрика: создание элемента в общем указателе (удобство построения дерева)
template <typename T, typename... Args>
ControlPtr make_control(Args&&... args) {
    return std::make_shared<T>(std::forward<Args>(args)...);
}

} // namespace engine::ui
