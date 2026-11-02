// engine/platform/sdl_window.cpp — реализация окна на SDL2 (FR-E01, FR-E02, FR-E20).
#include "platform/window.h"

#ifdef DND_ENGINE_WITH_SDL2

#include <SDL.h>

#include "core/log.h"

namespace engine::platform {

struct SdlWindow::Impl {
    SDL_Window* window = nullptr;
    SDL_GLContext gl_context = nullptr;
};

// Расчёт вьюпорта (формула (1)) вынесен в platform/window.cpp: он не зависит
// от SDL2 и тестируется headless (E-D11).

SdlWindow::SdlWindow() : impl_(new Impl()) {}

SdlWindow::~SdlWindow() {
    destroy();
    delete impl_;
    impl_ = nullptr;
}

bool SdlWindow::create(const WindowSettings& settings) {
    settings_ = settings;

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_EVENTS) != 0) {
        core::Log::instance().error(std::string("window: SDL_Init failed: ") + SDL_GetError());
        return false; // отказ без аварийного завершения (п. 4.2.3)
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, settings.gl_major);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, settings.gl_minor);
    if (settings.gl_core_profile) {
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    }
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);      // 2D: буфер глубины не требуется
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 0);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    std::uint32_t flags = SDL_WINDOW_OPENGL | SDL_WINDOW_ALLOW_HIGHDPI;
    if (settings.fullscreen) flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
    if (settings.resizable) flags |= SDL_WINDOW_RESIZABLE;

    impl_->window = SDL_CreateWindow(settings.title.c_str(), SDL_WINDOWPOS_CENTERED,
                                     SDL_WINDOWPOS_CENTERED,
                                     static_cast<int>(settings.window_width),
                                     static_cast<int>(settings.window_height), flags);
    if (impl_->window == nullptr) {
        core::Log::instance().error(std::string("window: SDL_CreateWindow failed: ") +
                                    SDL_GetError());
        return false;
    }

    impl_->gl_context = SDL_GL_CreateContext(impl_->window);
    if (impl_->gl_context == nullptr) {
        core::Log::instance().error(std::string("window: SDL_GL_CreateContext failed: ") +
                                    SDL_GetError());
        SDL_DestroyWindow(impl_->window);
        impl_->window = nullptr;
        return false;
    }

    SDL_GL_SetSwapInterval(settings.vsync ? 1 : 0);
    core::Log::instance().info("window: создано " + std::to_string(settings.window_width) + "x" +
                               std::to_string(settings.window_height) + ", OpenGL " +
                               std::to_string(settings.gl_major) + "." +
                               std::to_string(settings.gl_minor) +
                               (settings.gl_core_profile ? " Core" : ""));
    return true;
}

void SdlWindow::destroy() {
    if (impl_ == nullptr) return;
    if (impl_->gl_context != nullptr) {
        SDL_GL_DeleteContext(impl_->gl_context);
        impl_->gl_context = nullptr;
    }
    if (impl_->window != nullptr) {
        SDL_DestroyWindow(impl_->window);
        impl_->window = nullptr;
    }
}

void SdlWindow::swap() {
    if (impl_->window != nullptr) SDL_GL_SwapWindow(impl_->window);
}

void SdlWindow::set_title(const std::string& title) {
    settings_.title = title;
    if (impl_->window != nullptr) SDL_SetWindowTitle(impl_->window, title.c_str());
}

std::uint32_t SdlWindow::window_width() const {
    int w = 0;
    if (impl_->window != nullptr) SDL_GetWindowSize(impl_->window, &w, nullptr);
    return static_cast<std::uint32_t>(w);
}

std::uint32_t SdlWindow::window_height() const {
    int h = 0;
    if (impl_->window != nullptr) SDL_GetWindowSize(impl_->window, nullptr, &h);
    return static_cast<std::uint32_t>(h);
}

ui::ViewportInfo SdlWindow::on_resized(std::uint32_t width, std::uint32_t height) {
    settings_.window_width = width;
    settings_.window_height = height;
    return compute_viewport(width, height, settings_.logical_width, settings_.logical_height);
}

void* SdlWindow::native_handle() const { return impl_ != nullptr ? impl_->window : nullptr; }
void* SdlWindow::gl_context() const { return impl_ != nullptr ? impl_->gl_context : nullptr; }

} // namespace engine::platform

#endif // DND_ENGINE_WITH_SDL2
