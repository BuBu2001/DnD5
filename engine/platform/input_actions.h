// engine/platform/input_actions.h — маппинг «действие → привязки ввода» (FR-E03).
// Слой движка: имена действий — произвольные строки, семантику задаёт игровой
// слой (E-D01: движок не знает, что такое «конец хода» или «атака»).
// Схема привязок игры — game/controls/control_scheme.h, docs/controls.md.
#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "platform/input.h"

namespace engine::platform {

// Привязка действия: либо клавиша, либо кнопка мыши, либо ось колеса.
struct ActionBinding {
    enum class Source : std::uint8_t { Key, MouseButton, Wheel };
    Source source = Source::Key;
    Key key = Key::Unknown;
    MouseButton button = MouseButton::Left;
    int wheel_sign = 0; // +1 / −1 для Source::Wheel
};

class InputActions {
public:
    void bind(const std::string& action, ActionBinding binding) {
        bindings_[action].push_back(binding);
    }
    void bind_key(const std::string& action, Key key) {
        bind(action, ActionBinding{ActionBinding::Source::Key, key, MouseButton::Left, 0});
    }
    void bind_mouse(const std::string& action, MouseButton button) {
        bind(action, ActionBinding{ActionBinding::Source::MouseButton, Key::Unknown, button, 0});
    }
    void bind_wheel(const std::string& action, int sign) {
        bind(action, ActionBinding{ActionBinding::Source::Wheel, Key::Unknown, MouseButton::Left,
                                   sign > 0 ? 1 : -1});
    }

    // Пересчёт состояния действий за кадр: pressed — событие в текущем кадре,
    // held — удерживается (непрерывные действия: панорамирование камеры).
    void update(const InputState& input) {
        pressed_.clear();
        wheel_.clear();
        held_.clear();
        for (const auto& [action, list] : bindings_) {
            for (const auto& b : list) {
                switch (b.source) {
                    case ActionBinding::Source::Key:
                        if (input.is_key_pressed_this_frame(b.key)) pressed_.insert(action);
                        if (input.is_key_down(b.key)) held_.insert(action);
                        break;
                    case ActionBinding::Source::MouseButton:
                        if (input.is_mouse_pressed_this_frame(b.button)) pressed_.insert(action);
                        if (input.is_mouse_down(b.button)) held_.insert(action);
                        break;
                    case ActionBinding::Source::Wheel:
                        break; // колесо — только событийное, см. цикл ниже
                }
            }
        }
        // События колеса: знак дельты сопоставляется с привязкой
        for (const auto& e : input.frame_events()) {
            if (e.type != InputEvent::Type::MouseWheel || e.wheel_delta == 0) continue;
            const int sign = e.wheel_delta > 0 ? 1 : -1;
            for (const auto& [action, list] : bindings_) {
                for (const auto& b : list) {
                    if (b.source == ActionBinding::Source::Wheel && b.wheel_sign == sign) {
                        wheel_.insert(action);
                    }
                }
            }
        }
    }

    bool pressed(const std::string& action) const { return pressed_.count(action) != 0; }
    bool held(const std::string& action) const { return held_.count(action) != 0; }
    bool wheel(const std::string& action) const { return wheel_.count(action) != 0; }

    // Ось из четырёх направленных действий (−1…+1) — для панорамирования камеры
    float axis(const std::string& negative, const std::string& positive) const {
        float v = 0.f;
        if (held(negative)) v -= 1.f;
        if (held(positive)) v += 1.f;
        return v;
    }

    std::size_t action_count() const { return bindings_.size(); }

private:
    std::unordered_map<std::string, std::vector<ActionBinding>> bindings_;
    std::unordered_set<std::string> pressed_;
    std::unordered_set<std::string> held_;
    std::unordered_set<std::string> wheel_;
};

} // namespace engine::platform
