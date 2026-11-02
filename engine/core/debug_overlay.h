// engine/core/debug_overlay.h — отладочные инструменты (FR-E100…FR-E103).
// Панель показывает: частоту кадров, время кадра, число вызовов отрисовки,
// число сущностей, размер кэша ресурсов. Включение/отключение — горячей клавишей
// и параметром конфигурации (FR-E103); в сборке release отключена по умолчанию
// (п. 4.1.10.2).
//
// Движок не знает правил D&D (п. 1.5): счётчики — общие, без игровой семантики.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "render/renderer.h"

namespace engine::core {

struct FrameStats {
    double frame_time_ms = 0.0;      // время последнего кадра
    double average_fps = 0.0;        // скользящее среднее частоты кадров
    std::uint32_t draw_calls = 0;    // FR-E100, NFR-E01: не более 300
    std::uint32_t sprites_queued = 0;
    std::size_t entity_count = 0;
    std::size_t resource_cache_size = 0;
    std::uint32_t logic_steps_last_frame = 0;
    std::uint32_t dropped_logic_steps = 0;
};

class DebugOverlay {
public:
    // Бюджет кадра: t_budget = 1000 / f_target (формула (4), п. 4.1.3.4)
    static constexpr double kTargetFpsDesktop = 60.0;
    static constexpr double kTargetFpsMobile = 30.0;
    static constexpr std::uint32_t kMaxDrawCallsPerFrame = 300; // NFR-E01

    static double frame_budget_ms(double target_fps) { return 1000.0 / target_fps; }

    void set_enabled(bool enabled) { enabled_ = enabled; }
    bool enabled() const { return enabled_; }
    void toggle() { enabled_ = !enabled_; } // горячая клавиша (FR-E103)

    // Обновление скользящего среднего по времени кадра
    void update(double frame_time_ms) {
        last_frame_time_ms_ = frame_time_ms;
        const double fps = frame_time_ms > 0.0 ? 1000.0 / frame_time_ms : 0.0;
        // экспоненциальное сглаживание: альфа = 0.1
        average_fps_ = average_fps_ <= 0.0 ? fps : average_fps_ * 0.9 + fps * 0.1;
    }

    double average_fps() const { return average_fps_; }
    double last_frame_time_ms() const { return last_frame_time_ms_; }

    // Превышение бюджета кадра фиксируется (п. 4.1.3.4) и видно в панели
    bool over_frame_budget(double target_fps = kTargetFpsDesktop) const {
        return last_frame_time_ms_ > frame_budget_ms(target_fps);
    }

    bool over_draw_call_budget(std::uint32_t draw_calls) const {
        return draw_calls > kMaxDrawCallsPerFrame;
    }

    // Формирование строк панели (отрисовка текста — IRenderer::draw_text, FR-E29)
    FrameStats make_stats(std::uint32_t draw_calls, std::uint32_t sprites,
                          std::size_t entities, std::size_t cache_size,
                          std::uint32_t logic_steps, std::uint32_t dropped_steps) const {
        FrameStats s;
        s.frame_time_ms = last_frame_time_ms_;
        s.average_fps = average_fps_;
        s.draw_calls = draw_calls;
        s.sprites_queued = sprites;
        s.entity_count = entities;
        s.resource_cache_size = cache_size;
        s.logic_steps_last_frame = logic_steps;
        s.dropped_logic_steps = dropped_steps;
        return s;
    }

    std::string format_stats(const FrameStats& s) const;

    // Отрисовка панели в логических координатах 1280×720
    void draw(render::IRenderer& renderer, const FrameStats& stats) const;

private:
    bool enabled_ = false; // в release по умолчанию выключена (п. 4.1.10.2)
    double last_frame_time_ms_ = 0.0;
    double average_fps_ = 0.0;
};

} // namespace engine::core
