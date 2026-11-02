// engine/platform/main_loop.h — главный цикл с фиксированным шагом логики
// (п. 4.1.2.3–4.1.2.4 ТЗ DOC-04; формула (2): dt_logic = 1/f_logic, f_logic = 60 Гц).
#pragma once

#include <functional>

namespace engine::platform {

class MainLoop {
public:
    static constexpr double kLogicFrequencyHz = 60.0;      // f_logic
    static constexpr double kDtLogic = 1.0 / kLogicFrequencyHz; // dt_logic ≈ 16,67 мс
    static constexpr int kMaxStepsPerFrame = 5;            // ограничение «спирали смерти»

    using UpdateFn = std::function<void(double dt_logic)>;
    using RenderFn = std::function<void(double alpha)>;
    using PollFn = std::function<void()>;
    using RunningFn = std::function<bool()>;

    void set_update(UpdateFn fn) { update_ = std::move(fn); }
    void set_render(RenderFn fn) { render_ = std::move(fn); }
    void set_poll(PollFn fn) { poll_ = std::move(fn); }
    void set_running(RunningFn fn) { running_ = std::move(fn); }

    // Одна итерация цикла (контракт п. 4.1.2.4). Возвращает число шагов логики.
    int tick(double dt_frame);

    // Число пропущенных шагов при ограничении (для отладочных инструментов, FR-E100)
    int dropped_steps_last_frame() const { return dropped_steps_last_frame_; }

private:
    double accumulator_ = 0.0;
    int dropped_steps_last_frame_ = 0;
    UpdateFn update_;
    RenderFn render_;
    PollFn poll_;
    RunningFn running_ = [] { return true; };
};

} // namespace engine::platform
