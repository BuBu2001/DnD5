// engine/core/event_bus.h — событийная шина (FR-E10, контракт п. Г.10).
// События несут DTO, а не ссылки на внутренние структуры модуля правил (п. 4.1.11.6).
#pragma once

#include <algorithm>
#include <cstdint>
#include <functional>
#include <typeindex>
#include <unordered_map>
#include <vector>

namespace engine::core {

using SubscriptionId = std::uint64_t;

class EventBus {
public:
    template <typename Event>
    SubscriptionId subscribe(std::function<void(const Event&)> handler) {
        const auto idx = std::type_index(typeid(Event));
        const SubscriptionId id = next_id_++;
        handlers_[idx].push_back({id, [handler = std::move(handler)](const void* e) {
                                      handler(*static_cast<const Event*>(e));
                                  }});
        return id;
    }

    template <typename Event>
    void publish(const Event& event) {
        auto it = handlers_.find(std::type_index(typeid(Event)));
        if (it == handlers_.end()) return;
        // копия списка — допускается отписка внутри обработчика
        auto snapshot = it->second;
        for (const auto& entry : snapshot) {
            entry.fn(&event);
        }
    }

    template <typename Event>
    void unsubscribe(SubscriptionId id) {
        auto it = handlers_.find(std::type_index(typeid(Event)));
        if (it == handlers_.end()) return;
        auto& vec = it->second;
        vec.erase(std::remove_if(vec.begin(), vec.end(),
                                 [id](const Entry& e) { return e.id == id; }),
                  vec.end());
    }

private:
    struct Entry {
        SubscriptionId id;
        std::function<void(const void*)> fn;
    };

    std::unordered_map<std::type_index, std::vector<Entry>> handlers_;
    SubscriptionId next_id_ = 1;
};

} // namespace engine::core
