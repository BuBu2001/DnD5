// engine/ecs/events.h — отложенные команды ECS (паттерн command buffer, FR-E57).
// Создание/удаление сущностей и добавление/снятие компонентов во время обхода
// View запрещено менять Registry напрямую — команды буферизуются и исполняются
// в начале следующего шага логики (детерминизм, NFR-E09).
#pragma once

#include <functional>
#include <vector>

#include "ecs/registry.h"

namespace engine::ecs {

class CommandBuffer {
public:
    explicit CommandBuffer(Registry& registry) : registry_(registry) {}

    // Отложенное создание сущности: идентификатор будет записан в out
    // к моменту исполнения команд.
    EntityId create_entity(EntityId* out = nullptr) {
        commands_.push_back([this, out, &reg = registry_]() {
            const EntityId e = reg.create_entity();
            if (out) *out = e;
        });
        return kNullEntity; // действительный id появится после flush()
    }

    void destroy_entity(EntityId e) {
        commands_.push_back([&reg = registry_, e]() { reg.destroy_entity(e); });
    }

    template <typename T, typename... Args>
    void add_component(EntityId e, Args&&... args) {
        commands_.push_back([&reg = registry_, e,
                             tup = std::make_tuple(std::forward<Args>(args)...)]() mutable {
            std::apply([&](auto&&... a) { reg.template add_component<T>(e, std::move(a)...); },
                       std::move(tup));
        });
    }

    template <typename T>
    void remove_component(EntityId e) {
        commands_.push_back([&reg = registry_, e]() { reg.template remove_component<T>(e); });
    }

    // Произвольная отложенная команда
    void push(std::function<void()> fn) { commands_.push_back(std::move(fn)); }

    std::size_t pending() const { return commands_.size(); }

    void flush() {
        // команды могут добавлять новые команды — исполняем волнами
        while (!commands_.empty()) {
            std::vector<std::function<void()>> wave;
            wave.swap(commands_);
            for (auto& fn : wave) fn();
        }
    }

private:
    Registry& registry_;
    std::vector<std::function<void()>> commands_;
};

} // namespace engine::ecs
