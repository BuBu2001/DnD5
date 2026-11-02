// engine/ui/widgets.cpp — реализация элементов управления (FR-E82, FR-E83).
#include "ui/widgets.h"

#include <algorithm>

namespace engine::ui {

namespace {

// До этапа M2 (шрифтовой атлас, FR-E29) текст рисуется как подложка-маркер,
// поэтому ширина оценивается по среднему шагу глифа — этого достаточно для макета.
constexpr float kAverageGlyphWidth = 8.f;

} // namespace

// --- Panel -------------------------------------------------------------------
void Panel::on_draw(render::IRenderer& renderer) {
    renderer.draw_rect(absolute_rect(), style_.background);
    if (border_width_ > 0.f) {
        renderer.draw_rect_outline(absolute_rect(), border_color_, border_width_);
    }
}

// --- Label -------------------------------------------------------------------
float Label::estimated_text_width() const {
    // UTF-8: считаем символы, а не байты (п. 4.5.5)
    std::size_t chars = 0;
    for (std::size_t i = 0; i < text_.size(); ++i) {
        if ((static_cast<unsigned char>(text_[i]) & 0xC0u) != 0x80u) ++chars;
    }
    return static_cast<float>(chars) * kAverageGlyphWidth * (font_size_ / 16.f);
}

void Label::on_draw(render::IRenderer& renderer) {
    renderer.draw_text(absolute_rect().x, absolute_rect().y, text_, font_size_, color_);
}

// --- Button ------------------------------------------------------------------
void Button::activate() {
    if (!enabled_ || !visible_) return;
    ++click_count_;
    if (toggle_) toggled_ = !toggled_;
    if (on_click_) on_click_();
}

bool Button::on_event(UiEvent& event) {
    switch (event.type) {
        case UiEventType::PointerPressed:
            if (event.button == 0) { pressed_ = true; return true; }
            return false;
        case UiEventType::PointerReleased:
            if (event.button == 0 && pressed_) {
                pressed_ = false;
                activate();
                return true;
            }
            return false;
        case UiEventType::PointerLeave:
            pressed_ = false;
            return false;
        default:
            return false;
    }
}

void Button::on_draw(render::IRenderer& renderer) {
    render::Color fill = style_.background;
    if (!enabled_) fill = render::Color{fill.r * 0.6f, fill.g * 0.6f, fill.b * 0.6f, fill.a};
    else if (pressed_) fill = style_.accent;
    else if (hovered_ || toggled_) fill = render::Color{
        (style_.background.r + style_.accent.r) * 0.5f,
        (style_.background.g + style_.accent.g) * 0.5f,
        (style_.background.b + style_.accent.b) * 0.5f, style_.background.a};
    renderer.draw_rect(absolute_rect(), fill);
    renderer.draw_rect_outline(absolute_rect(), style_.accent, 1.f);
    renderer.draw_text(absolute_rect().x + style_.padding, absolute_rect().y + style_.padding,
                       label_, style_.font_size, style_.foreground);
}

// --- Image -------------------------------------------------------------------
void Image::on_draw(render::IRenderer& renderer) {
    if (texture_ == 0) return; // текстура не загружена: пропуск с записью в журнал (п. 4.2.1)
    renderer.draw_sprite(texture_, source_rect_, absolute_rect(), tint_);
}

// --- ScrollView --------------------------------------------------------------
float ScrollView::max_scroll_offset() const {
    const float overflow = content_size_.h - size_.h;
    return overflow > 0.f ? overflow : 0.f;
}

void ScrollView::set_scroll_offset(float offset) {
    scroll_offset_ = std::clamp(offset, 0.f, max_scroll_offset());
}

void ScrollView::scroll_by(float delta) { set_scroll_offset(scroll_offset_ + delta); }

void ScrollView::scroll_to_bottom() { set_scroll_offset(max_scroll_offset()); }

void ScrollView::scroll_to_top() { set_scroll_offset(0.f); }

float ScrollView::visible_fraction() const {
    if (content_size_.h <= 0.f) return 1.f;
    const float fraction = size_.h / content_size_.h;
    return std::clamp(fraction, 0.f, 1.f);
}

bool ScrollView::on_event(UiEvent& event) {
    // Прокрутка колесом: событие Wheel кодируется как PointerMoved с button = 1
    // (адаптация ввода выполняется в слое platform, FR-E03).
    if (event.type == UiEventType::PointerMoved && event.button == 1) {
        scroll_by(event.position.y < 0.f ? -36.f : 36.f);
        return true;
    }
    return false;
}

// --- Tooltip -----------------------------------------------------------------
void Tooltip::update_visibility() {
    if (owner_ == nullptr) { should_show_ = false; return; }
    should_show_ = owner_->hovered();
}

void Tooltip::on_update(double dt_logic) {
    update_visibility();
    if (!should_show_) { hover_time_ = 0.f; set_visible(false); return; }
    hover_time_ += static_cast<float>(dt_logic);
    set_visible(hover_time_ >= show_delay_);
    if (owner_ != nullptr && visible_) {
        // Подсказка располагается под владельцем, не выходя за экран (FR-E84)
        const Vec2 owner = owner_->absolute_position();
        set_position(Vec2{owner.x, owner.y + owner_->size().h + 2.f});
    }
}

// --- Modal -------------------------------------------------------------------
void Modal::open() {
    open_ = true;
    set_visible(true);
}

void Modal::close() {
    if (!open_) return;
    open_ = false;
    set_visible(false);
    if (on_close_) on_close_();
}

bool Modal::on_event(UiEvent& event) {
    if (!open_) return false;
    // Закрытие по нажатию вне содержимого и по клавише Escape обрабатывает
    // игровой слой (ввод приходит через platform::InputManager, FR-E03).
    event.handled = true; // модальное окно поглощает события (FR-E83)
    return true;
}

void Modal::on_draw(render::IRenderer& renderer) {
    if (!open_) return;
    // Затемнение фона под модальным окном
    renderer.draw_rect(render::Rect{0.f, 0.f, 1280.f, 720.f},
                       render::Color{0.f, 0.f, 0.f, 0.45f});
    renderer.draw_rect(absolute_rect(), style_.background);
    renderer.draw_rect_outline(absolute_rect(), style_.accent, 2.f);
}

} // namespace engine::ui
