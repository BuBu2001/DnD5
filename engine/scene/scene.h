// engine/scene/scene.h — интерфейс сцены (FR-E70…FR-E79, слой State/Scene).
#pragma once

#include "ecs/registry.h"

namespace engine::scene {

class Scene {
public:
    virtual ~Scene() = default;
    virtual void on_enter() {}
    virtual void on_exit() {}
    // Вызываются при приостановке/возобновлении сцены под ней в стеке (FR-E70)
    // Базовая реализация устанавливает флаг; производные сцены обязаны вызывать её
    virtual void on_pause() { paused_ = true; }
    virtual void on_resume() { paused_ = false; }
    virtual void update(double dt_logic) = 0;
    // alpha — интерполяция между шагами логики (п. 4.1.2.4)
    virtual void render(double alpha) = 0;

    bool is_paused() const { return paused_; }
    void set_paused(bool paused) {
        if (paused_ == paused) return;
        if (paused) on_pause(); else on_resume();
    }

    ecs::Registry& registry() { return registry_; }
    const ecs::Registry& registry() const { return registry_; }

private:
    ecs::Registry registry_;
    bool paused_ = false;
};

} // namespace engine::scene
