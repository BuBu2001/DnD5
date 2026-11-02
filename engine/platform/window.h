// engine/platform/window.h — интерфейс окна (заглушка до этапа M1; SDL2, таблица 4.16).
// Логическое разрешение 1280x720; масштаб S = min(W_win/W_log, H_win/H_log) (п. 4.1.2.2, формула (1)).
#pragma once

#include <string>

namespace engine::platform {

struct LogicalResolution {
    int width = 1280;
    int height = 720;
};

class IWindow {
public:
    virtual ~IWindow() = default;
    virtual bool create(const std::string& title, int physical_w, int physical_h) = 0;
    virtual void destroy() = 0;
    virtual void swap_buffers() = 0;
    virtual bool should_close() const = 0;

    // Коэффициент масштабирования по формуле (1)
    static float compute_scale(int window_w, int window_h, const LogicalResolution& logical) {
        const float sx = static_cast<float>(window_w) / static_cast<float>(logical.width);
        const float sy = static_cast<float>(window_h) / static_cast<float>(logical.height);
        return sx < sy ? sx : sy;
    }
};

} // namespace engine::platform
