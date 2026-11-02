// engine/platform/timer.h — таймер кадра (FR-Exx, слой платформы).
#pragma once

#include <chrono>

namespace engine::platform {

class Timer {
public:
    Timer() { reset(); }

    void reset() { start_ = std::chrono::steady_clock::now(); }

    // Время с момента последнего reset()/elapsed-вызова, сек (dt_frame, п. 4.1.2.4)
    double elapsed_and_restart() {
        const auto now = std::chrono::steady_clock::now();
        std::chrono::duration<double> d = now - start_;
        start_ = now;
        return d.count();
    }

    double elapsed() const {
        const auto now = std::chrono::steady_clock::now();
        std::chrono::duration<double> d = now - start_;
        return d.count();
    }

private:
    std::chrono::steady_clock::time_point start_;
};

} // namespace engine::platform
