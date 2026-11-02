// engine/platform/input.h — опрос ввода (клавиатура/мышь; геймпад и сенсор — V1).
#pragma once

#include <cstdint>
#include <unordered_map>

namespace engine::platform {

enum class Key : std::uint16_t { Unknown = 0, Escape, Enter, Space, Left, Right, Up, Down, A, D, W, S };
enum class MouseButton : std::uint8_t { Left, Right, Middle };

class InputState {
public:
    void set_key(Key key, bool pressed) { keys_[static_cast<int>(key)] = pressed; }
    bool is_key_down(Key key) const {
        auto it = keys_.find(static_cast<int>(key));
        return it != keys_.end() && it->second;
    }

    void set_mouse_position(float x, float y) { mouse_x_ = x; mouse_y_ = y; }
    float mouse_x() const { return mouse_x_; }
    float mouse_y() const { return mouse_y_; }

    void set_mouse_button(MouseButton b, bool pressed) { mouse_[static_cast<int>(b)] = pressed; }
    bool is_mouse_down(MouseButton b) const {
        auto it = mouse_.find(static_cast<int>(b));
        return it != mouse_.end() && it->second;
    }

    void begin_frame() {} // вызывается из input.poll() главного цикла

private:
    std::unordered_map<int, bool> keys_;
    std::unordered_map<int, bool> mouse_;
    float mouse_x_ = 0.f;
    float mouse_y_ = 0.f;
};

} // namespace engine::platform
