// engine/ecs/command_buffer.h — отложенные команды ECS-lite (FR-E64, E-D06).
// Добавление/удаление сущностей и компонентов во время итерации View запрещено
// применять к Registry напрямую: команды буферизуются и исполняются на flush()
// в детерминированном порядке (NFR-E09).
//
// DeferredEntity решает проблему «сущность ещё не создана, а компонент уже
// заказан»: идентификатор проставляется в момент flush(), все последующие
// команды с этим маркером автоматически подставляют реальный EntityId.
#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <tuple>
#include <utility>
#include <vector>

#include "ecs/entity.h"
#include "ecs/registry.h"

namespace engine::ecs {

// Маркер сущности, создаваемой отложенно. Копируется по значению, владеет
// общим слотом результата (std::shared_ptr), поэтому безопасен при захвате в
// несколько отложенных команд.
class DeferredEntity {
public:
    DeferredEntity() : slot_(std::make_shared<EntityId>(kNullEntity)) {}

    bool resolved() const { return *slot_ != kNullEntity; }
    EntityId id() const { return *slot_; }

    // Метод const: запись идёт не в сам маркер, а в разделяемый слот результата,
    // поэтому вызов допустим из const-захвата лямбды отложенной команды.
    void set(EntityId e) const { *slot_ = e; }

    friend bool operator==(const DeferredEntity& a, const DeferredEntity& b) {
        return a.slot_ == b.slot_;
    }

private:
    std::shared_ptr<EntityId> slot_;
};

class CommandBuffer {
public:
    explicit CommandBuffer(Registry& registry) : registry_(registry) {}

    CommandBuffer(const CommandBuffer&) = delete;
    CommandBuffer& operator=(const CommandBuffer&) = delete;

    // --- отложенное создание -------------------------------------------------
    // Возвращает маркер; реальный идентификатор доступен после flush()
    // через DeferredEntity::id() или через необязательный указатель out.
    DeferredEntity create_entity(EntityId* out = nullptr) {
        DeferredEntity handle;
        commands_.push_back([this, handle, out]() {
            const EntityId e = registry_.create_entity();
            handle.set(e);
            if (out != nullptr) *out = e;
        });
        return handle;
    }

    void destroy_entity(EntityId e) {
        commands_.push_back([this, e]() { registry_.destroy_entity(e); });
    }

    void destroy_entity(const DeferredEntity& e) {
        commands_.push_back([this, e]() {
            if (const EntityId id = e.id(); id != kNullEntity) registry_.destroy_entity(id);
        });
    }

    // --- отложенные компоненты ----------------------------------------------
    template <typename T, typename... Args>
    void add_component(EntityId e, Args&&... args) {
        commands_.push_back(
            [this, e, tup = std::make_tuple(std::forward<Args>(args)...)]() mutable {
                std::apply([this, e](auto&&... a) { registry_.add_component<T>(e, a...); }, tup);
            });
    }

    template <typename T, typename... Args>
    void add_component(const DeferredEntity& e, Args&&... args) {
        commands_.push_back(
            [this, e, tup = std::make_tuple(std::forward<Args>(args)...)]() mutable {
                const EntityId id = e.id();
                if (id == kNullEntity) return; // сущность не создана — команда отклоняется
                std::apply([this, id](auto&&... a) { registry_.add_component<T>(id, a...); }, tup);
            });
    }

    template <typename T>
    void remove_component(EntityId e) {
        commands_.push_back([this, e]() { registry_.remove_component<T>(e); });
    }

    template <typename T>
    void remove_component(const DeferredEntity& e) {
        commands_.push_back([this, e]() {
            if (const EntityId id = e.id(); id != kNullEntity) registry_.remove_component<T>(id);
        });
    }

    // Произвольная отложенная команда (точки расширения, FR-E105)
    void push(std::function<void()> fn) { commands_.push_back(std::move(fn)); }

    std::size_t pending() const { return commands_.size(); }
    bool empty() const { return commands_.empty(); }

    // Исполнение команд. Команды могут добавлять новые команды — исполняем
    // волнами; kMaxWaves защищает от бесконечного цикла при ошибке логики.
    void flush() {
        for (std::uint32_t wave = 0; wave < kMaxWaves && !commands_.empty(); ++wave) {
            std::vector<std::function<void()>> batch;
            batch.swap(commands_);
            for (auto& fn : batch) fn();
        }
        commands_.clear(); // остаток отбрасываем: превышен лимит волн
    }

    static inline constexpr std::uint32_t kMaxWaves = 8;

private:
    Registry& registry_;
    std::vector<std::function<void()>> commands_;
};

} // namespace engine::ecs
