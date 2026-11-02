#include "platform/main_loop.h"

namespace engine::platform {

int MainLoop::tick(double dt_frame) {
    if (poll_) poll_();                 // input.poll()

    accumulator_ += dt_frame;
    int steps = 0;
    while (accumulator_ >= kDtLogic && steps < kMaxStepsPerFrame) {
        if (update_) update_(kDtLogic); // scene_stack.update(dt_logic)
        accumulator_ -= kDtLogic;
        ++steps;
    }
    dropped_steps_last_frame_ = 0;
    // при просадках ограничиваем накопление, чтобы не разгонять «спираль смерти»
    if (accumulator_ >= kDtLogic) {
        while (accumulator_ >= kDtLogic) {
            accumulator_ -= kDtLogic;
            ++dropped_steps_last_frame_;
        }
    }

    const double alpha = accumulator_ / kDtLogic;
    if (render_) render_(alpha);        // scene_stack.render(alpha)
    return steps;
}

} // namespace engine::platform
