// engine/ecs/entity.h — идентификатор сущности ECS-lite (FR-E55…FR-E60).
// Младшие 32 бита — индекс, старшие — поколение (версия) для проверки живости.
#pragma once

#include <cstdint>
#include <functional>

namespace engine::ecs {

struct EntityId {
    std::uint32_t index = 0;
    std::uint32_t generation = 0;

    friend bool operator==(const EntityId&, const EntityId&) = default;
    friend bool operator<(const EntityId& a, const EntityId& b) {
        return std::tie(a.index, a.generation) < std::tie(b.index, b.generation);
    }
};

inline constexpr EntityId kNullEntity{0xFFFFFFFFu, 0xFFFFFFFFu};

} // namespace engine::ecs

namespace std {
template <>
struct hash<engine::ecs::EntityId> {
    size_t operator()(const engine::ecs::EntityId& e) const noexcept {
        return (static_cast<size_t>(e.generation) << 32) | e.index;
    }
};
} // namespace std
