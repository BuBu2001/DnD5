// engine/ecs/system.h — интерфейс системы ECS-lite (поведение — только в системах).
#pragma once

namespace engine::ecs {

class Registry;

class ISystem {
public:
    virtual ~ISystem() = default;
    virtual void update(Registry& registry, double dt_logic) = 0;
};

} // namespace engine::ecs
