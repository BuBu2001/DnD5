// engine/ecs/registry.h — реестр ECS-lite: сущности, компоненты, системы.
#pragma once

#include <memory>
#include <typeindex>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "ecs/component_storage.h"
#include "ecs/entity.h"
#include "ecs/system.h"

namespace engine::ecs {

class Registry {
public:
    EntityId create_entity() {
        std::uint32_t index;
        if (!free_list_.empty()) {
            index = free_list_.back();
            free_list_.pop_back();
        } else {
            index = static_cast<std::uint32_t>(generations_.size());
            generations_.push_back(0);
        }
        alive_.insert(index);
        return EntityId{index, generations_[index]};
    }

    bool is_alive(EntityId e) const {
        return e.index < generations_.size() && generations_[e.index] == e.generation &&
               alive_.count(e.index) != 0;
    }

    void destroy_entity(EntityId e) {
        if (!is_alive(e)) return;
        for (auto& [type, storage] : storages_) storage->remove(e);
        alive_.erase(e.index);
        ++generations_[e.index];
        free_list_.push_back(e.index);
    }

    template <typename T, typename... Args>
    T& add_component(EntityId e, Args&&... args) {
        return storage<T>().emplace(e, std::forward<Args>(args)...);
    }

    template <typename T>
    void remove_component(EntityId e) {
        storage<T>().remove(e);
    }

    template <typename T>
    bool has_component(EntityId e) const {
        auto it = storages_.find(std::type_index(typeid(T)));
        return it != storages_.end() && it->second->has(e);
    }

    template <typename T>
    T& get_component(EntityId e) {
        return storage<T>().get(e);
    }

    template <typename T>
    T* try_get_component(EntityId e) {
        return storage<T>().try_get(e);
    }

    template <typename T>
    ComponentStorage<T>& storage() {
        const auto idx = std::type_index(typeid(T));
        auto it = storages_.find(idx);
        if (it == storages_.end()) {
            it = storages_.emplace(idx, std::make_unique<ComponentStorage<T>>()).first;
        }
        return *static_cast<ComponentStorage<T>*>(it->second.get());
    }

    template <typename S, typename... Args>
    S& add_system(Args&&... args) {
        systems_.push_back(std::make_unique<S>(*this, std::forward<Args>(args)...));
        return *static_cast<S*>(systems_.back().get());
    }

    void update_systems(double dt) {
        for (auto& s : systems_) s->update(*this, dt);
    }

private:
    std::vector<std::uint32_t> generations_;
    std::vector<std::uint32_t> free_list_;
    std::unordered_set<std::uint32_t> alive_; // set индексов живых сущностей
    std::unordered_map<std::type_index, std::unique_ptr<IComponentStorage>> storages_;
    std::vector<std::unique_ptr<ISystem>> systems_;
};

} // namespace engine::ecs
