// game/combat/combat_scene.h — боевая сцена вертикального среза (этап M1 demo).
// Связывает engine (ECS, сцены, события) и rules (броски, здоровье, сетка) через
// игровой слой: только game имеет право знать оба мира (направления зависимостей, п. 3.3.2).
#pragma once

#include <string>
#include <vector>

#include "core/event_bus.h"
#include "ecs/command_buffer.h"
#include "render/camera.h"
#include "rules/attack.h"
#include "rules/dice_roller.h"
#include "rules/grid.h"
#include "rules/health.h"
#include "rules/initiative.h"
#include "rules/random_provider.h"
#include "scene/scene.h"

namespace game {

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
};

class CombatScene : public engine::scene::Scene {
public:
    struct Combatant {
        std::string name;
        bool player_side = true;
        rules::HealthState health;
        int attack_mod = 0;
        int dex_mod = 0;
        int ac = 10;
        rules::Cell cell{};
        engine::ecs::EntityId entity{};
    };

    CombatScene(engine::core::EventBus& bus, rules::DiceRoller& roller)
        : bus_(bus), roller_(roller), cmds_(registry()), grid_(16, 12) {}

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
        if (outcome.hit) {
            damage = roll_damage(outcome.critical);
            rules::apply_damage(target.health, damage);
        }

        AttackResolvedEvent ev{attacker.name, target.name, outcome.hit,
                               outcome.critical, damage, outcome.description};
        bus_.publish(ev);
        log_.push_back(ev.description);
        return ev;
    }

    const std::vector<std::string>& combat_log() const { return log_; }
    rules::TacticalGrid& grid() { return grid_; }
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

    // 1d8 урона; при критическом попадании кость удваивается (DOC-01)
    int roll_damage(bool critical) {
        const int dice = critical ? 2 : 1;
        int total = 0;
        for (int i = 0; i < dice; ++i) {
            total += static_cast<int>(rng_.next_u32() % 8u) + 1;
        }
        return total;
    }

    engine::core::EventBus& bus_;
    rules::DiceRoller& roller_;
    rules::SeededRandomProvider rng_{1337};
    engine::ecs::CommandBuffer cmds_;
    rules::TacticalGrid grid_;
    engine::render::Camera2D camera_;
    rules::InitiativeQueue initiative_;
    std::vector<Combatant> combatants_;
    std::vector<engine::ecs::EntityId> entity_ids_;
    std::vector<std::string> log_;
};

} // namespace game
