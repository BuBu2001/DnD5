// engine/core/debug_overlay.cpp — отладочная панель (FR-E100…FR-E103).
#include "core/debug_overlay.h"

#include <cstdio>
#include <string>

namespace engine::core {

std::string DebugOverlay::format_stats(const FrameStats& s) const {
    char buf[512];
    std::snprintf(buf, sizeof(buf),
                  "FPS %.1f  frame %.2f ms (budget %.2f)%s\n"
                  "draw calls %u%s  sprites %u\n"
                  "entities %zu  resource cache %zu\n"
                  "logic steps %u  dropped %u",
                  s.average_fps, s.frame_time_ms, frame_budget_ms(kTargetFpsDesktop),
                  over_frame_budget() ? " OVER" : "",
                  s.draw_calls,
                  over_draw_call_budget(s.draw_calls) ? " OVER BUDGET" : "",
                  s.sprites_queued, s.entity_count, s.resource_cache_size,
                  s.logic_steps_last_frame, s.dropped_logic_steps);
    return std::string(buf);
}

void DebugOverlay::draw(render::IRenderer& renderer, const FrameStats& stats) const {
    if (!enabled_) return; // п. 4.1.10.2: отключённая панель не влияет на поведение
    const render::Rect panel{8.f, 8.f, 360.f, 92.f};
    renderer.draw_rect(panel, render::Color{0.f, 0.f, 0.f, 0.55f});
    renderer.draw_rect_outline(panel,
                               over_draw_call_budget(stats.draw_calls)
                                   ? render::Color{0.9f, 0.3f, 0.25f, 1.f}
                                   : render::Color{0.4f, 0.8f, 0.4f, 1.f},
                               1.f);

    float y = panel.y + 6.f;
    const auto line = [&](const std::string& text) {
        renderer.draw_text(panel.x + 8.f, y, text, 13.f, render::Color{0.9f, 0.95f, 0.9f, 1.f});
        y += 18.f;
    };
    char buf[128];
    std::snprintf(buf, sizeof(buf), "FPS %.1f   frame %.2f ms", stats.average_fps,
                  stats.frame_time_ms);
    line(buf);
    std::snprintf(buf, sizeof(buf), "draw calls %u   sprites %u", stats.draw_calls,
                  stats.sprites_queued);
    line(buf);
    std::snprintf(buf, sizeof(buf), "entities %zu   cache %zu", stats.entity_count,
                  stats.resource_cache_size);
    line(buf);
    std::snprintf(buf, sizeof(buf), "logic steps %u   dropped %u", stats.logic_steps_last_frame,
                  stats.dropped_logic_steps);
    line(buf);
}

} // namespace engine::core
