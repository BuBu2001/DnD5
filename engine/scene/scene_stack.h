// engine/scene/scene_stack.h — стек сцен: push/pop/replace, update/render вершины.
#pragma once

#include <memory>
#include <vector>

#include "scene/scene.h"

namespace engine::scene {

class SceneStack {
public:
    void push(std::unique_ptr<Scene> scene) {
        stack_.push_back(std::move(scene));
        stack_.back()->on_enter();
    }

    void pop() {
        if (stack_.empty()) return;
        stack_.back()->on_exit();
        stack_.pop_back();
    }

    void replace(std::unique_ptr<Scene> scene) {
        pop();
        push(std::move(scene));
    }

    bool empty() const { return stack_.empty(); }
    std::size_t depth() const { return stack_.size(); }
    Scene* top() { return stack_.empty() ? nullptr : stack_.back().get(); }

    void update(double dt_logic) {
        if (!stack_.empty()) stack_.back()->update(dt_logic);
    }

    void render(double alpha) {
        if (!stack_.empty()) stack_.back()->render(alpha);
    }

private:
    std::vector<std::unique_ptr<Scene>> stack_;
};

} // namespace engine::scene
