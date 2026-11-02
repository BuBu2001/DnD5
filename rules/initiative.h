// rules/initiative.h — очередь инициативы с tie-breaker (п. 4.1.11.3, п. Г.6).
#pragma once

#include <algorithm>
#include <cstdint>
#include <vector>

namespace rules {

using EntityId = std::uint64_t; // упрощённый идентификатор для правил (DTO-совместимый)

struct InitiativeEntry {
    EntityId entity = 0;
    int score = 0;        // бросок d20 + модификатор
    int dex_modifier = 0;
    std::uint64_t tie_breaker = 0; // 1) больший DEX mod; 2) меньший EntityId
};

class InitiativeQueue {
public:
    void build(std::vector<InitiativeEntry> entries) {
        for (auto& e : entries) {
            e.tie_breaker = (static_cast<std::uint64_t>(e.dex_modifier + 128) << 32);
        }
        std::sort(entries.begin(), entries.end(), [](const InitiativeEntry& a,
                                                     const InitiativeEntry& b) {
            if (a.score != b.score) return a.score > b.score;
            if (a.dex_modifier != b.dex_modifier) return a.dex_modifier > b.dex_modifier;
            return a.entity < b.entity; // меньший EntityId выше (детерминизм, NFR-E09)
        });
        entries_ = std::move(entries);
        cursor_ = 0;
    }

    InitiativeEntry next() {
        if (entries_.empty()) return {};
        const auto e = entries_[cursor_ % entries_.size()];
        ++cursor_;
        return e;
    }

    bool contains(EntityId id) const {
        return std::any_of(entries_.begin(), entries_.end(),
                           [id](const InitiativeEntry& e) { return e.entity == id; });
    }

    std::size_t size() const { return entries_.size(); }

private:
    std::vector<InitiativeEntry> entries_;
    std::size_t cursor_ = 0;
};

} // namespace rules
