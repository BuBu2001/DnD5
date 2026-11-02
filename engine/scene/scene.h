// engine/scene/scene.h — интерфейс сцены (FR-E70…FR-E79, слой State/Scene).
#pragma once

#include "ecs/registry.h"

namespace engine::scene {

class Scene {
public:
    virtual ~Scene() = default;
    virtual void on_enter() {}
    virtual void on_exit() {}
    virtual void update(double dt_logic) = 0;
    // alpha — интерполяция между шагами логики (п. 4.1.2.4)
    virtual void render(double alpha) = 0;

    ecs::Registry& registry() { return registry_; }
    const ecs::Registry& registry() const { return registry_; }

private:
    ecs::Registry registry_;
};

} // namespace engine::scene
