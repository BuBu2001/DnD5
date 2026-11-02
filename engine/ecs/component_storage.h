// engine/ecs/component_storage.h — хранилище компонентов одного типа.
// Компоненты хранят данные и не содержат поведения (п. 4.1.6.3).
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

#include "ecs/entity.h"

namespace engine::ecs {

class IComponentStorage {
public:
    virtual ~IComponentStorage() = default;
    virtual void remove(EntityId entity) = 0;
    virtual bool has(EntityId entity) const = 0;
    virtual std::size_t size() const = 0;
};

template <typename T>
class ComponentStorage final : public IComponentStorage {
public:
    template <typename... Args>
    T& emplace(EntityId entity, Args&&... args) {
        auto [it, inserted] = components_.try_emplace(
            entity.index, Entry{entity.generation, std::forward<Args>(args)...});
        if (!inserted) it->second = Entry{entity.generation, T{std::forward<Args>(args)...}};
        return it->second.value;
    }

    T& get(EntityId entity) { return components_.at(entity.index).value; }
    const T& get(EntityId entity) const { return components_.at(entity.index).value; }

    T* try_get(EntityId entity) {
        auto it = components_.find(entity.index);
        if (it == components_.end() || it->second.generation != entity.generation) return nullptr;
        return &it->second.value;
    }

    const T* try_get(EntityId entity) const {
        auto it = components_.find(entity.index);
        if (it == components_.end() || it->second.generation != entity.generation) return nullptr;
        return &it->second.value;
    }

    bool has(EntityId entity) const override { return try_get(entity) != nullptr; }
    void remove(EntityId entity) override { components_.erase(entity.index); }
    std::size_t size() const override { return components_.size(); }

    // Обход всех живых компонентов (для View)
    template <typename Fn>
    void for_each(Fn&& fn) {
        for (auto& [index, entry] : components_) {
            fn(EntityId{index, entry.generation}, entry.value);
        }
    }

private:
    struct Entry {
        std::uint32_t generation;
        T value;
    };
    std::unordered_map<std::uint32_t, Entry> components_;
};

} // namespace engine::ecs
