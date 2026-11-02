// engine/platform/window.h — окно и графический контекст (FR-E01, FR-E02, FR-E20).
// Абстракция поверх SDL2: создание окна, логическое разрешение, масштабирование
// с сохранением пропорций и полями letterbox/pillarbox (формула (1), п. 4.1.2.2).
// Реализация SDL2 — platform/sdl_window.cpp, собирается при DND_WITH_SDL2=ON;
// без SDL2 доступен расчёт вьюпорта (Window::compute_viewport) — он тестируется headless.
#pragma once

#include <cstdint>
#include <string>

#include "ui/ui_root.h"

namespace engine::platform {

struct WindowSettings {
    std::string title = "dnd_engine";
    std::uint32_t window_width = 1280;   // физический размер окна
    std::uint32_t window_height = 720;
    std::uint32_t logical_width = ui::UiRoot::kLogicalWidth;   // 1280 (FR-E01)
    std::uint32_t logical_height = ui::UiRoot::kLogicalHeight; // 720
    bool fullscreen = false;
    bool vsync = true;
    bool resizable = true;
    int gl_major = 3;   // OpenGL 3.3 Core Profile на ПК (FR-E20)
    int gl_minor = 3;
    bool gl_core_profile = true;
};

class Window {
public:
    virtual ~Window() = default;

    // Создание окна и графического контекста. Возвращает false при ошибке —
    // аварийное завершение не допускается (п. 4.2.3).
    virtual bool create(const WindowSettings& settings) = 0;
    virtual void destroy() = 0;
    virtual void swap() = 0;                    // SDL_GL_SwapWindow (п. 4.1.2.4)
    virtual void set_title(const std::string& title) = 0;

    virtual std::uint32_t window_width() const = 0;
    virtual std::uint32_t window_height() const = 0;

    // Обработка изменения размера окна: пересчёт масштаба и полей (FR-E02)
    virtual ui::ViewportInfo on_resized(std::uint32_t width, std::uint32_t height) = 0;

    // Расчёт вьюпорта по формуле (1) — чистая функция, тестируется без окна
    static ui::ViewportInfo compute_viewport(std::uint32_t window_w, std::uint32_t window_h,
                                             std::uint32_t logical_w, std::uint32_t logical_h);
};

// SDL2-реализация окна (этап M1). Объявлена всегда, определена при DND_WITH_SDL2.
class SdlWindow final : public Window {
public:
    SdlWindow();
    ~SdlWindow() override;

    bool create(const WindowSettings& settings) override;
    void destroy() override;
    void swap() override;
    void set_title(const std::string& title) override;

    std::uint32_t window_width() const override;
    std::uint32_t window_height() const override;
    ui::ViewportInfo on_resized(std::uint32_t width, std::uint32_t height) override;

    // Доступ к нативному дескриптору для рендер-бэкенда (E-D04)
    void* native_handle() const;
    void* gl_context() const;
    const WindowSettings& settings() const { return settings_; }

private:
    struct Impl;
    Impl* impl_ = nullptr;
    WindowSettings settings_{};
};

} // namespace engine::platform
