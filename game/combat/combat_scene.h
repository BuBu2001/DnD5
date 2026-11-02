// game/combat/combat_scene.h — боевая сцена вертикального среза (этап M1 demo).
// Связывает engine (ECS, сцены, события) и rules (броски, здоровье, сетка) через
// игровой слой: только game имеет право знать оба мира (направления зависимостей, п. 3.3.2).
#pragma once

#include <functional>
#include <queue>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "core/event_bus.h"
#include "ecs/command_buffer.h"
#include "render/camera.h"
#include "rules/attack.h"
#include "rules/dice_roller.h"
#include "rules/grid.h"
#include "rules/health.h"
#include "rules/initiative.h"
#include "rules/damage.h"
#include "rules/dto_convert.h"
#include "scene/scene.h"

namespace game {

// Компаратор приоритетной очереди маршрута: только по стоимости футов
// (rules::Cell намеренно не имеет порядка — только равенство).
struct ByCost {
    bool operator()(const std::pair<int, rules::Cell>& a,
                    const std::pair<int, rules::Cell>& b) const {
        return a.first > b.first;
    }
};

// Компоненты ECS игрового слоя (данные без поведения, п. 4.1.6.3)
struct GridPosition {
    int cx = 0, cy = 0;
};

struct CombatantTag {
    std::string name;
    bool is_player_side = true;
};

// Событие движковой шины (несёт значения, не ссылки на структуры правил, п. 4.1.11.6)
struct AttackResolvedEvent {
    std::string attacker;
    std::string target;
    bool hit = false;
    bool critical = false;
    int damage = 0;
    std::string description;
    std::string damage_notation;                 // "1d8+2" — для журнала боя
    rules::RollBreakdownDTO attack_breakdown;    // DTO для панели разбора (UI-E02, E-D12)
};

class CombatScene : public engine::scene::Scene {
public:
    struct Combatant {
        std::string name;
        bool player_side = true;
        rules::HealthState health;
        rules::DamageResistance resistance; // сопротивления/уязвимости/иммунитеты (FR-K41)
        int attack_mod = 0;
        int dex_mod = 0;
        int ac = 10;
        int speed_feet = 30;                    // скорость хода (DOC-01, п. 2.3)
        int movement_remaining_feet = 0;        // остаток движения в футах
        bool action_used = false;               // действие атаки потрачено (п. Г.6)
        rules::Cell cell{};
        engine::ecs::EntityId entity{};
    };

    CombatScene(engine::core::EventBus& bus, rules::DiceRoller& roller,
                int grid_w = 16, int grid_h = 12)
        : bus_(bus), roller_(roller), cmds_(registry()), grid_(grid_w, grid_h) {}

    void add_combatant(const std::string& name, bool player_side, int hp_max,
                       int attack_mod, int dex_mod, int ac, int x, int y) {
        Combatant c;
        c.name = name;
        c.player_side = player_side;
        c.health.hp_max = hp_max;
        c.health.hp_current = hp_max;
        c.attack_mod = attack_mod;
        c.dex_mod = dex_mod;
        c.ac = ac;
        c.cell = {x, y};
        combatants_.push_back(std::move(c));
    }

    void on_enter() override {
        // Сущности создаются отложенными командами (FR-E57), затем исполняются разом
        entity_ids_.assign(combatants_.size(), engine::ecs::kNullEntity);
        for (std::size_t i = 0; i < combatants_.size(); ++i) {
            cmds_.create_entity(&entity_ids_[i]);
        }
        cmds_.flush();
        for (std::size_t i = 0; i < combatants_.size(); ++i) {
            auto& reg = registry();
            combatants_[i].entity = entity_ids_[i];
            reg.add_component<GridPosition>(entity_ids_[i],
                                            combatants_[i].cell.x, combatants_[i].cell.y);
            reg.add_component<CombatantTag>(entity_ids_[i],
                                            combatants_[i].name, combatants_[i].player_side);
        }
        build_initiative();
    }

    void update(double /*dt_logic*/) override {
        cmds_.flush(); // исполнение отложенных команд в начале шага логики
    }

    void render(double /*alpha*/) override {
        // headless-прогон: камера неподвижна, спрайты рисует M2-рендерер
    }

    // Ход атакующего по цели (управление вводом появится на этапе M1+)
    AttackResolvedEvent perform_attack(std::size_t attacker_idx, std::size_t target_idx) {
        auto& attacker = combatants_[attacker_idx];
        auto& target = combatants_[target_idx];

        rules::AttackInput in;
        in.ability_modifier = attacker.attack_mod;
        const auto outcome = rules::resolve_attack(roller_, in, target.ac);

        int damage = 0;
        rules::DamageRollResult damage_roll;
        if (outcome.hit) {
            damage_roll = roll_damage(outcome.critical, attacker.attack_mod / 2);
            // Множители сопротивлений применяются ПОСЛЕ сложения (DOC-01 FR-K42)
            damage = rules::apply_resistance(damage_roll.total, rules::DamageType::Slashing,
                                             target.resistance);
            rules::apply_damage(target.health, damage);
        }

        attacker.action_used = true; // атака расходует действие (п. Г.6)

        // Событие несёт DTO и значения, а не ссылки на структуры правил (п. 4.1.11.6)
        AttackResolvedEvent ev;
        ev.attacker = attacker.name;
        ev.target = target.name;
        ev.hit = outcome.hit;
        ev.critical = outcome.critical;
        ev.damage = damage;
        ev.description = outcome.description;
        ev.damage_notation = damage_roll.notation;
        ev.attack_breakdown = rules::to_dto(outcome.roll, "Атака: " + attacker.name);
        bus_.publish(ev);
        log_.push_back(ev.description);
        return ev;
    }

    // Перемещение бойца по тактической сетке (п. 4.1.8): ортогональный маршрут,
    // стоимость клеток — rules::TacticalGrid (сложная местность ×2), занятые
    // клетки блокируют путь. Движение тратит футы, действие не расходует.
    struct MoveResult {
        bool ok = false;
        int cost_feet = 0;
        std::string reason;
    };
    MoveResult move_unit(std::size_t idx, const rules::Cell& to) {
        MoveResult result;
        if (idx >= combatants_.size()) { result.reason = "нет бойца"; return result; }
        auto& unit = combatants_[idx];
        if (unit.health.hp_current <= 0) { result.reason = "боец без сознания"; return result; }
        if (to == unit.cell) { result.ok = true; return result; }
        if (!grid_.in_bounds(to) || !grid_.is_passable(to)) {
            result.reason = "клетка непроходима"; return result;
        }
        if (occupied_by(to, idx) != kNoIndex) { result.reason = "клетка занята"; return result; }

        const auto path = find_path(unit.cell, to, unit.movement_remaining_feet, idx);
        if (path.cost_feet < 0) { result.reason = "не хватает движения"; return result; }
        unit.cell = to;
        unit.movement_remaining_feet -= path.cost_feet;
        if (unit.entity != engine::ecs::kNullEntity &&
            registry().has_component<GridPosition>(unit.entity)) {
            auto& gp = registry().get_component<GridPosition>(unit.entity);
            gp.cx = to.x;
            gp.cy = to.y;
        }
        result.ok = true;
        result.cost_feet = path.cost_feet;
        return result;
    }

    // Клетки, достижимые оставшимся движением (подсветка зон, основа FR-E35):
    // ортогональный обход, стоимость входа — entry_cost_feet, занятые клетки — стоп.
    std::vector<rules::Cell> reachable_cells(std::size_t idx) const {
        std::vector<rules::Cell> out;
        if (idx >= combatants_.size()) return out;
        const auto& unit = combatants_[idx];
        if (unit.health.hp_current <= 0 || unit.movement_remaining_feet <= 0) return out;

        std::unordered_map<long long, int> best; // ключ (x<<16|y) → футы
        std::priority_queue<std::pair<int, rules::Cell>,
                            std::vector<std::pair<int, rules::Cell>>, ByCost> frontier;
        frontier.push({0, unit.cell});
        best[cell_key(unit.cell)] = 0;
        while (!frontier.empty()) {
            const auto [cost, cur] = frontier.top();
            frontier.pop();
            if (cost > best[cell_key(cur)]) continue;
            static const int dx[4] = {1, -1, 0, 0};
            static const int dy[4] = {0, 0, 1, -1};
            for (int d = 0; d < 4; ++d) {
                const rules::Cell next{cur.x + dx[d], cur.y + dy[d]};
                if (!grid_.in_bounds(next) || !grid_.is_passable(next)) continue;
                if (occupied_by(next, idx) != kNoIndex) continue;
                const int next_cost = cost + grid_.entry_cost_feet(next);
                if (next_cost > unit.movement_remaining_feet) continue;
                const auto key = cell_key(next);
                const auto it = best.find(key);
                if (it != best.end() && it->second <= next_cost) continue;
                best[key] = next_cost;
                frontier.push({next_cost, next});
                out.push_back(next);
            }
        }
        return out;
    }

    // Индекс бойца в клетке (или kNoIndex); except — исключить из поиска
    static constexpr std::size_t kNoIndex = static_cast<std::size_t>(-1);
    std::size_t occupied_by(const rules::Cell& c, std::size_t except = kNoIndex) const {
        for (std::size_t i = 0; i < combatants_.size(); ++i) {
            if (i != except && combatants_[i].cell == c && combatants_[i].health.hp_current > 0) {
                return i;
            }
        }
        return kNoIndex;
    }
    std::size_t combatant_at(const rules::Cell& c) const { return occupied_by(c); }

    // Перестройка сетки под размеры карты (до on_enter): сцена неизменяема
    // (ссылочные члены), поэтому состав бойцов и сетка сбрасываются на месте.
    void reset_grid(int width, int height) {
        grid_ = rules::TacticalGrid(width, height);
        combatants_.clear();
        log_.clear();
    }

    // Начало хода бойца: пополнить движение, сбросить действие (reset_turn, п. Г.6)
    void begin_turn(std::size_t idx) {
        if (idx < combatants_.size()) {
            auto& unit = combatants_[idx];
            unit.movement_remaining_feet = unit.speed_feet;
            unit.action_used = false;
        }
    }

    Combatant& combatant_mut(std::size_t i) { return combatants_[i]; }
    const rules::InitiativeQueue& initiative() const { return initiative_; }
    engine::core::EventBus& bus() { return bus_; }
    rules::DiceRoller& roller() { return roller_; }

    const std::vector<std::string>& combat_log() const { return log_; }
    rules::TacticalGrid& grid() { return grid_; }
    const rules::TacticalGrid& grid() const { return grid_; }
    engine::render::Camera2D& camera() { return camera_; }
    std::size_t combatant_count() const { return combatants_.size(); }
    const Combatant& combatant(std::size_t i) const { return combatants_[i]; }

private:
    void build_initiative() {
        std::vector<rules::InitiativeEntry> entries;
        for (std::size_t i = 0; i < combatants_.size(); ++i) {
            const auto r = roller_.roll(0, combatants_[i].dex_mod, false);
            entries.push_back({static_cast<rules::EntityId>(i), r.total,
                               combatants_[i].dex_mod, 0});
        }
        initiative_.build(std::move(entries));
    }

    // 1d8+модификатор урона через ЕДИНУЮ точку броска DiceRoller (FR-K102, AC-K22):
    // прямой вызов генератора случайных чисел в боевом коде запрещён.
    // При критическом попадании удваиваются кости, а не модификатор (DOC-01).
    rules::DamageRollResult roll_damage(bool critical, int modifier) {
        return roller_.roll_damage(/*dice_count=*/1, /*dice_sides=*/8, modifier, critical);
    }

    // Поиск ортогонального маршрута (Dijkstra по стоимости футов); cost_feet < 0 —
    // маршрут дороже остатка движения. Реализация — игровой слой (rules/grid.h:
    // «расчёт пути (A*) — игровой слой»).
    struct PathCost { int cost_feet = -1; };
    PathCost find_path(const rules::Cell& from, const rules::Cell& to, int budget_feet,
                       std::size_t mover_idx) const {
        std::unordered_map<long long, int> best;
        std::priority_queue<std::pair<int, rules::Cell>,
                            std::vector<std::pair<int, rules::Cell>>, ByCost> frontier;
        frontier.push({0, from});
        best[cell_key(from)] = 0;
        while (!frontier.empty()) {
            const auto [cost, cur] = frontier.top();
            frontier.pop();
            if (cur == to) return PathCost{cost};
            if (cost > best[cell_key(cur)]) continue;
            static const int dx[4] = {1, -1, 0, 0};
            static const int dy[4] = {0, 0, 1, -1};
            for (int d = 0; d < 4; ++d) {
                const rules::Cell next{cur.x + dx[d], cur.y + dy[d]};
                if (!grid_.in_bounds(next) || !grid_.is_passable(next)) continue;
                // Через занятые клетки проходить нельзя (кроме цели — она проверялась выше)
                if (next != to && occupied_by(next, mover_idx) != kNoIndex) continue;
                const int next_cost = cost + grid_.entry_cost_feet(next);
                if (next_cost > budget_feet) continue;
                const auto key = cell_key(next);
                const auto it = best.find(key);
                if (it != best.end() && it->second <= next_cost) continue;
                best[key] = next_cost;
                frontier.push({next_cost, next});
            }
        }
        return PathCost{-1};
    }

    static long long cell_key(const rules::Cell& c) {
        return (static_cast<long long>(c.x) << 16) | static_cast<unsigned short>(c.y);
    }

    engine::core::EventBus& bus_;
    rules::DiceRoller& roller_;
    engine::ecs::CommandBuffer cmds_;
    rules::TacticalGrid grid_;
    engine::render::Camera2D camera_;
    rules::InitiativeQueue initiative_;
    std::vector<Combatant> combatants_;
    std::vector<engine::ecs::EntityId> entity_ids_;
    std::vector<std::string> log_;
};

} // namespace game
