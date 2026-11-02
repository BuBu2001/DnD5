// engine/platform/input.h — опрос ввода: клавиатура, мышь (кнопки, перемещение, колесо).
// FR-E03; геймпад (FR-E04) и сенсорный ввод (FR-E05) — статус V1.
// Слой ввода не знает правил D&D (п. 1.5): только коды клавиш и координаты.
#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>

#include "ui/control.h"
#include "ui/ui_root.h"
#include "platform/window.h"

namespace engine::platform {

enum class Key : std::uint16_t {
    Unknown = 0, Escape, Enter, Space, Left, Right, Up, Down,
    A, D, W, S, F1, F2, F3, Tab, Shift, Ctrl, Plus, Minus, Zero,
    // Игровые привязки схемы управления (docs/controls.md, FR-E03):
    Digit1, Digit2, Digit3, Digit4, Digit5, Digit6, Digit7, Digit8, Digit9,
    Q, E, R, C, I, L, M
};

enum class MouseButton : std::uint8_t { Left, Right, Middle };

// Кадровое событие ввода: «нажато в этом кадре» / «отпущено в этом кадре»
struct InputEvent {
    enum class Type : std::uint8_t { KeyDown, KeyUp, MouseDown, MouseUp, MouseMove, MouseWheel };
    Type type = Type::MouseMove;
    Key key = Key::Unknown;
    MouseButton button = MouseButton::Left;
    float x = 0.f, y = 0.f;      // позиция курсора в логических координатах 1280×720
    int wheel_delta = 0;          // прокрутка колеса (FR-E03), знако-ориентированная
};

class InputState {
public:
    void set_key(Key key, bool pressed) { keys_[static_cast<int>(key)] = pressed; }
    bool is_key_down(Key key) const {
        const auto it = keys_.find(static_cast<int>(key));
        return it != keys_.end() && it->second;
    }

    void set_mouse_position(float x, float y) { mouse_x_ = x; mouse_y_ = y; }
    float mouse_x() const { return mouse_x_; }
    float mouse_y() const { return mouse_y_; }

    void set_mouse_button(MouseButton b, bool pressed) { mouse_[static_cast<int>(b)] = pressed; }
    bool is_mouse_down(MouseButton b) const {
        const auto it = mouse_.find(static_cast<int>(b));
        return it != mouse_.end() && it->second;
    }

    // Нажата в текущем кадре (для однократных действий: выбор, подтверждение)
    bool is_key_pressed_this_frame(Key key) const;
    bool is_mouse_pressed_this_frame(MouseButton b) const;

    // Кадровый цикл: begin_frame() фиксирует снимок, end_frame() очищает «нажатия кадра»
    void begin_frame();
    void end_frame();

    // Регистрация события кадра. KeyDown/MouseDown дополнительно фиксируются
    // как «нажато в этом кадре» (контракт input_frame_cycle, FR-E03); снимок
    // очищается в begin_frame()/end_frame().
    void push_event(const InputEvent& event) {
        switch (event.type) {
            case InputEvent::Type::KeyDown:
                keys_pressed_frame_[static_cast<int>(event.key)] = true;
                break;
            case InputEvent::Type::MouseDown:
                mouse_pressed_frame_[static_cast<int>(event.button)] = true;
                break;
            default:
                break;
        }
        frame_events_.push_back(event);
    }
    const std::vector<InputEvent>& frame_events() const { return frame_events_; }
    void clear_events() { frame_events_.clear(); }

    std::size_t event_count() const { return frame_events_.size(); }

private:
    std::unordered_map<int, bool> keys_;
    std::unordered_map<int, bool> mouse_;
    std::unordered_map<int, bool> keys_pressed_frame_;
    std::unordered_map<int, bool> mouse_pressed_frame_;
    std::vector<InputEvent> frame_events_;
    float mouse_x_ = 0.f;
    float mouse_y_ = 0.f;
};

// Преобразование событий ввода в события UI-дерева (FR-E80): координаты окна
// переводятся в логические координаты разметки (FR-E02, FR-E84).
class InputToUiAdapter {
public:
    // Возвращает список событий UI для dispatch_event() корня UI
    static std::vector<ui::UiEvent> translate(const InputState& input, const ui::UiRoot& ui_root);

    // Событие колеса мыши: кодируется как PointerMoved с button = 1 (см. ScrollView)
    static ui::UiEvent wheel_event(int delta, ui::Vec2 logical_position);
};

// Опрос очереди событий окна за один кадр (контракт п. 4.1.2.4: input.poll()).
// Реализация — platform/sdl_input.cpp (SDL2); возвращает false при получении
// события выхода, после чего главный цикл завершается штатно (п. 4.2.3).
bool poll_sdl_events(InputState& state, Window& window, ui::UiRoot& ui_root);

} // namespace engine::platform
