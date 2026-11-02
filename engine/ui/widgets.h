// engine/ui/widgets.h — элементы управления (FR-E82, FR-E83): панель, метка, кнопка,
// изображение, область прокрутки, всплывающая подсказка, модальное окно.
// Все виджеты работают только с данными (строки, числа, цвета): знания о правилах
// D&D в слое UI запрещены (UI-E01, E-D02).
#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "render/renderer.h"
#include "ui/control.h"

namespace engine::ui {

// Панель: прямоугольная подложка с рамкой (FR-E82)
class Panel : public Control {
public:
    void set_border_color(render::Color c) { border_color_ = c; }
    void set_border_width(float w) { border_width_ = w; }
    render::Color border_color() const { return border_color_; }
    float border_width() const { return border_width_; }

protected:
    void on_draw(render::IRenderer& renderer) override;

private:
    render::Color border_color_{0.25f, 0.28f, 0.34f, 1.0f};
    float border_width_ = 1.f;
};

// Метка: текст и цвет (FR-E82). Отрисовка текста — через контракт IRenderer::draw_text
// (шрифтовой атлас, FR-E29, этап M2); до его появления метка рисует подложку.
class Label : public Control {
public:
    Label() = default;
    explicit Label(std::string text) : text_(std::move(text)) {}

    void set_text(std::string text) { text_ = std::move(text); }
    const std::string& text() const { return text_; }
    void set_color(render::Color c) { color_ = c; }
    render::Color color() const { return color_; }
    void set_font_size(float size) { font_size_ = size; }
    float font_size() const { return font_size_; }

    // Оценка ширины текста в логических единицах (для автоподбора размера до M2)
    float estimated_text_width() const;

protected:
    void on_draw(render::IRenderer& renderer) override;

private:
    std::string text_;
    render::Color color_{0.92f, 0.93f, 0.95f, 1.0f};
    float font_size_ = 16.f;
};

// Кнопка: нажатие, удержание, отключение, обработчик клика (FR-E82)
class Button : public Control {
public:
    using ClickHandler = std::function<void()>;

    Button() = default;
    explicit Button(std::string text) : label_(std::move(text)) {}

    void set_text(std::string text) { label_ = std::move(text); }
    const std::string& text() const { return label_; }
    void set_on_click(ClickHandler handler) { on_click_ = std::move(handler); }

    // Программное нажатие (тесты, горячие клавиши, доступность)
    void activate();

    std::size_t click_count() const { return click_count_; }
    bool is_toggle() const { return toggle_; }
    void set_toggle(bool on) { toggle_ = on; }
    bool toggled() const { return toggled_; }
    void set_toggled(bool value) { toggled_ = value; }

protected:
    bool on_event(UiEvent& event) override;
    void on_draw(render::IRenderer& renderer) override;

private:
    std::string label_;
    ClickHandler on_click_;
    std::size_t click_count_ = 0;
    bool toggle_ = false;
    bool toggled_ = false;
};

// Изображение: идентификатор текстуры и область в атласе (FR-E82, FR-E24)
class Image : public Control {
public:
    void set_texture(render::TextureId id) { texture_ = id; }
    render::TextureId texture() const { return texture_; }
    void set_source_rect(render::Rect r) { source_rect_ = r; }
    const render::Rect& source_rect() const { return source_rect_; }
    void set_tint(render::Color c) { tint_ = c; }

protected:
    void on_draw(render::IRenderer& renderer) override;

private:
    render::TextureId texture_ = 0;
    render::Rect source_rect_{};
    render::Color tint_{};
};

// Область прокрутки (FR-E82, FR-E87: журнал событий с прокруткой)
class ScrollView : public Control {
public:
    void set_content_size(Size s) { content_size_ = s; }
    Size content_size() const { return content_size_; }
    void set_scroll_offset(float offset);
    float scroll_offset() const { return scroll_offset_; }
    float max_scroll_offset() const;
    void scroll_by(float delta);       // колесо мыши (FR-E03)
    void scroll_to_bottom();           // журнал боя: автопрокрутка к последнему событию
    void scroll_to_top();
    float visible_fraction() const;    // доля видимого содержимого (для ползунка)

protected:
    bool on_event(UiEvent& event) override;
    bool blocks_children() const override { return false; }

private:
    Size content_size_{};
    float scroll_offset_ = 0.f;
};

// Всплывающая подсказка (FR-E83). Текст — данные; знание о правилах запрещено (UI-E01).
class Tooltip : public Control {
public:
    void set_text(std::string text) { text_ = std::move(text); }
    const std::string& text() const { return text_; }
    // Элемент-владелец подсказки: показывается при наведении на него
    void attach_to(Control* owner) { owner_ = owner; }
    void update_visibility();
    bool should_show() const { return should_show_; }

protected:
    void on_update(double dt_logic) override;

private:
    std::string text_;
    Control* owner_ = nullptr;
    bool should_show_ = false;
    float hover_time_ = 0.f;
    float show_delay_ = 0.35f; // задержка показа, чтобы подсказка не мигала
};

// Модальное окно (FR-E83): блокирует события остального дерева, пока открыто
class Modal : public Control {
public:
    void open();
    void close();
    bool is_open() const { return open_; }
    void set_on_close(std::function<void()> handler) { on_close_ = std::move(handler); }

protected:
    // Модальное окно перехватывает все события внутри своей области
    bool blocks_children() const override { return true; }
    bool on_event(UiEvent& event) override;
    void on_draw(render::IRenderer& renderer) override;

private:
    bool open_ = false;
    std::function<void()> on_close_;
};

} // namespace engine::ui
