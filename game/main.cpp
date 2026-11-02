// game/main.cpp — пример вертикального прогона каркаса (без окна, этап M0/M1).
// Демонстрирует: главный цикл с фиксированным шагом, ECS-lite, стек сцен,
// событийную шину и вызовы модуля правил через DTO-границу слоёв.
#include <cstdio>
#include <memory>

#include "core/event_bus.h"
#include "core/log.h"
#include "ecs/registry.h"
#include "ecs/view.h"
#include "platform/main_loop.h"
#include "rules/dice_roller.h"
#include "rules/random_provider.h"
#include "scene/scene_stack.h"

using namespace engine;

struct TransformComponent {
    float x = 0.f, y = 0.f;
};

// Событие-пример (движок не знает D&D-терминов, п. 1.5)
struct TurnStateChanged {
    int state = 0;
};

class DemoScene : public scene::Scene {
public:
    void on_enter() override {
        auto& reg = registry();
        e1_ = reg.create_entity();
        reg.add_component<TransformComponent>(e1_, 1.f, 2.f);
        e2_ = reg.create_entity();
        reg.add_component<TransformComponent>(e2_, 3.f, 4.f);
    }

    void update(double dt_logic) override {
        steps_ += dt_logic;
        for (auto e : ecs::make_view<TransformComponent>(registry())) {
            auto& t = registry().get_component<TransformComponent>(e);
            t.x += 1.f * dt_logic; // простейшая «система движения»
        }
    }

    void render(double alpha) override {
        if (!printed_) {
            std::printf("[demo] render alpha=%.3f entities=%zu\n", alpha,
                        ecs::make_view<TransformComponent>(registry()).size());
            printed_ = true;
        }
    }

private:
    ecs::EntityId e1_, e2_;
    double steps_ = 0.0;
    bool printed_ = false;
};

int main() {
    core::Log::instance().info("dnd_engine V0.2 — каркас движка (АБВГ.00004)");

    core::EventBus bus;
    bus.subscribe<TurnStateChanged>([](const TurnStateChanged& e) {
        std::printf("[bus] TurnStateChanged: %d\n", e.state);
    });
    bus.publish(TurnStateChanged{1});

    rules::SeededRandomProvider rng(42);
    rules::DiceRoller roller(rng);
    const auto result = roller.roll(/*dc=*/15, /*modifier=*/5, /*advantage=*/true);
    std::printf("[rules] d20=[%d,%d] taken=%d total=%d success=%s\n",
                result.dice[0], result.dice[1], result.taken, result.total,
                result.success ? "yes" : "no");

    scene::SceneStack scenes;
    scenes.push(std::make_unique<DemoScene>());

    platform::MainLoop loop;
    loop.set_update([&](double dt) { scenes.update(dt); });
    loop.set_render([&](double alpha) { scenes.render(alpha); });

    // имитация 10 кадров по 1/30 с (просадка: 60 Гц логика → 2 шага на кадр)
    for (int i = 0; i < 10; ++i) {
        loop.tick(1.0 / 30.0);
    }
    core::Log::instance().info("main loop finished OK");
    return 0;
}
