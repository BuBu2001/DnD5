// engine/ui/control.h — базовый виджет retained-mode UI (FR-E80…FR-E95).
// Заглушка до этапа M8. UI не знает правил D&D: оперирует числами и строками (E-D01).
#pragma once

#include <string>

#include "render/renderer.h"

namespace engine::ui {

struct Vec2 { float x = 0.f, y = 0.f; };
struct Size { float w = 0.f, h = 0.f; };

class Control {
public:
    virtual ~Control() = default;
    virtual void update(double dt_logic) {}
    virtual void draw(render::IRenderer& renderer) {}

    void set_position(Vec2 p) { position_ = p; }
    void set_size(Size s) { size_ = s; }
    Vec2 position() const { return position_; }
    Size size() const { return size_; }

    void set_visible(bool v) { visible_ = v; }
    bool visible() const { return visible_; }

protected:
    Vec2 position_{};
    Size size_{};
    bool visible_ = true;
    std::string name_;
};

} // namespace engine::ui
