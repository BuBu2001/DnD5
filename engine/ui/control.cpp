// engine/ui/control.cpp — реализация retained-mode UI-дерева (FR-E80, FR-E84).
#include "ui/control.h"

#include <algorithm>

namespace engine::ui {

void Control::add_child(ControlPtr child) {
    if (!child || child.get() == this) return;
    if (child->parent_ != nullptr) child->parent_->remove_child(child);
    child->parent_ = this;
    children_.push_back(std::move(child));
}

void Control::remove_child(const ControlPtr& child) {
    const auto it = std::find(children_.begin(), children_.end(), child);
    if (it == children_.end()) return;
    (*it)->parent_ = nullptr;
    children_.erase(it);
}

void Control::clear_children() {
    for (auto& child : children_) child->parent_ = nullptr;
    children_.clear();
}

Vec2 Control::absolute_position() const {
    Vec2 p = position_;
    for (const Control* node = parent_; node != nullptr; node = node->parent_) {
        p.x += node->position_.x;
        p.y += node->position_.y;
    }
    return p;
}

render::Rect Control::absolute_rect() const {
    const Vec2 p = absolute_position();
    return render::Rect{p.x, p.y, size_.w, size_.h};
}

bool Control::hit_test(Vec2 root_point) const {
    if (!visible_) return false;
    const Vec2 p = absolute_position();
    return root_point.x >= p.x && root_point.x <= p.x + size_.w &&
           root_point.y >= p.y && root_point.y <= p.y + size_.h;
}

void Control::update_all(double dt_logic) {
    if (!visible_) return;
    on_update(dt_logic);
    for (auto& child : children_) child->update_all(dt_logic);
}

void Control::draw_all(render::IRenderer& renderer) {
    if (!visible_) return;
    on_draw(renderer);
    // Порядок отрисовки: сначала сам элемент, затем потомки (FR-E27, FR-E80)
    for (auto& child : children_) child->draw_all(renderer);
}

bool Control::dispatch_event(UiEvent& event) {
    if (!visible_) return false;

    const bool inside = hit_test(event.root_position);
    if (inside && !hovered_) {
        hovered_ = true;
        UiEvent enter;
        enter.type = UiEventType::PointerEnter;
        enter.root_position = event.root_position;
        enter.position = to_local(event.root_position);
        on_event(enter);
    } else if (!inside && hovered_) {
        hovered_ = false;
        UiEvent leave;
        leave.type = UiEventType::PointerLeave;
        leave.root_position = event.root_position;
        on_event(leave);
    }

    if (!inside) return false;

    event.position = to_local(event.root_position);
    if (enabled_ && on_event(event)) {
        event.handled = true;
        return true;
    }
    if (blocks_children()) return event.handled;

    // Обход потомков в обратном порядке: верхние элементы получают событие первыми
    for (auto it = children_.rbegin(); it != children_.rend(); ++it) {
        if ((*it)->dispatch_event(event)) return true;
    }
    return event.handled;
}

Control* Control::find_by_name(const std::string& target) {
    if (name_ == target) return this;
    for (auto& child : children_) {
        if (Control* found = child->find_by_name(target); found != nullptr) return found;
    }
    return nullptr;
}

std::size_t Control::subtree_size() const {
    std::size_t total = 1;
    for (const auto& child : children_) total += child->subtree_size();
    return total;
}

} // namespace engine::ui
