// engine/ecs/view.h — обзор сущностей по набору компонентов.
#pragma once

#include <tuple>
#include <vector>

#include "ecs/entity.h"
#include "ecs/registry.h"

namespace engine::ecs {

// View<A, B>: сущности, у которых есть все перечисленные компоненты.
template <typename... Ts>
std::vector<EntityId> make_view(Registry& registry) {
    std::vector<EntityId> result;
    // обход первого хранилища с проверкой наличия остальных
    using First = std::tuple_element_t<0, std::tuple<Ts...>>;
    registry.storage<First>().for_each(
        [&](EntityId e, First&) {
            bool has_all = ((registry.try_get_component<Ts>(e) != nullptr) && ...);
            if (has_all) result.push_back(e);
        });
    return result;
}

} // namespace engine::ecs
