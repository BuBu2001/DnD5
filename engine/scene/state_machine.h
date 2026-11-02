// engine/scene/state_machine.h — универсальный конечный автомат с guard-условиями.
// Автомат хода (WaitingForAction → Targeting → Confirming → SpendingResources →
// Resolving → EndTurn) строится поверх этого класса (п. 4.1.7.2); сам движок
// не знает правил D&D (E-D01) — состояния задаются строковыми идентификаторами.
#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace engine::scene {

using StateId = std::uint32_t;

class StateMachine {
public:
    struct Transition {
        StateId from;
        StateId to;
        std::string trigger;
        std::function<bool()> guard; // пустой = всегда истинно
    };

    explicit StateMachine(StateId initial) : current_(initial) {}

    void add_transition(StateId from, StateId to, std::string trigger,
                        std::function<bool()> guard = {}) {
        transitions_[trigger].push_back(
            Transition{from, to, trigger, std::move(guard)});
    }

    bool dispatch(const std::string& trigger) {
        auto it = transitions_.find(trigger);
        if (it == transitions_.end()) return false;
        for (const auto& t : it->second) {
            if (t.from == current_ && (!t.guard || t.guard())) {
                on_state_change_(current_, t.to);
                current_ = t.to;
                return true;
            }
        }
        return false;
    }

    StateId current() const { return current_; }
    void set_on_state_change(std::function<void(StateId, StateId)> fn) {
        on_state_change_ = std::move(fn);
    }

private:
    StateId current_;
    std::unordered_map<std::string, std::vector<Transition>> transitions_;
    std::function<void(StateId, StateId)> on_state_change_ = [](StateId, StateId) {};
};

} // namespace engine::scene
