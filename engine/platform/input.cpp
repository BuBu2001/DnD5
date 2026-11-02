// engine/platform/input.cpp — состояние ввода и адаптация в события UI (FR-E03, FR-E80).
#include "platform/input.h"

namespace engine::platform {

bool InputState::is_key_pressed_this_frame(Key key) const {
    const auto it = keys_pressed_frame_.find(static_cast<int>(key));
    return it != keys_pressed_frame_.end() && it->second;
}

bool InputState::is_mouse_pressed_this_frame(MouseButton b) const {
    const auto it = mouse_pressed_frame_.find(static_cast<int>(b));
    return it != mouse_pressed_frame_.end() && it->second;
}

void InputState::begin_frame() {
    keys_pressed_frame_.clear();
    mouse_pressed_frame_.clear();
    frame_events_.clear();
}

void InputState::end_frame() {
    keys_pressed_frame_.clear();
    mouse_pressed_frame_.clear();
}

ui::UiEvent InputToUiAdapter::wheel_event(int delta, ui::Vec2 logical_position) {
    ui::UiEvent event;
    // Событие колеса кодируется как PointerMoved с button = 1: ScrollView
    // трактует знак y как направление прокрутки (FR-E87 — журнал с прокруткой)
    event.type = ui::UiEventType::PointerMoved;
    event.button = 1;
    event.position = logical_position;
    event.root_position = logical_position;
    (void)delta;
    event.position.y = delta > 0 ? 1.f : -1.f;
    return event;
}

std::vector<ui::UiEvent> InputToUiAdapter::translate(const InputState& input,
                                                     const ui::UiRoot& ui_root) {
    std::vector<ui::UiEvent> out;
    for (const auto& e : input.frame_events()) {
        const ui::Vec2 logical = ui_root.window_to_logical(e.x, e.y);
        ui::UiEvent ui_event;
        ui_event.root_position = logical;
        ui_event.position = logical;
        switch (e.type) {
            case InputEvent::Type::MouseMove:
                ui_event.type = ui::UiEventType::PointerMoved;
                break;
            case InputEvent::Type::MouseDown:
                ui_event.type = ui::UiEventType::PointerPressed;
                ui_event.button = static_cast<std::uint8_t>(e.button);
                break;
            case InputEvent::Type::MouseUp:
                ui_event.type = ui::UiEventType::PointerReleased;
                ui_event.button = static_cast<std::uint8_t>(e.button);
                break;
            case InputEvent::Type::MouseWheel:
                ui_event = wheel_event(e.wheel_delta, logical);
                break;
            case InputEvent::Type::KeyDown:
            case InputEvent::Type::KeyUp:
                continue; // клавиатура обрабатывается игровым слоем, не UI-деревом
        }
        out.push_back(ui_event);
    }
    return out;
}

} // namespace engine::platform
