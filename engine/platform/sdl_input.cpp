// engine/platform/sdl_input.cpp — опрос устройств ввода через SDL2 (FR-E03).
// Собирается при DND_WITH_SDL2=ON. Клавиатура, мышь (кнопки, перемещение, колесо);
// геймпад (FR-E04) и сенсорный ввод (FR-E05) — статус V1, этап M11.
#include "platform/input.h"

#ifdef DND_ENGINE_WITH_SDL2

#include <SDL.h>

#include "core/log.h"
#include "platform/window.h"

namespace engine::platform {

namespace {

Key translate_key(SDL_Keycode code) {
    switch (code) {
        case SDLK_ESCAPE: return Key::Escape;
        case SDLK_RETURN:
        case SDLK_KP_ENTER: return Key::Enter;
        case SDLK_SPACE:  return Key::Space;
        case SDLK_LEFT:   return Key::Left;
        case SDLK_RIGHT:  return Key::Right;
        case SDLK_UP:     return Key::Up;
        case SDLK_DOWN:   return Key::Down;
        case SDLK_a:      return Key::A;
        case SDLK_d:      return Key::D;
        case SDLK_w:      return Key::W;
        case SDLK_s:      return Key::S;
        case SDLK_F1:     return Key::F1;
        case SDLK_F2:     return Key::F2;
        case SDLK_F3:     return Key::F3;
        case SDLK_TAB:    return Key::Tab;
        case SDLK_LSHIFT:
        case SDLK_RSHIFT: return Key::Shift;
        case SDLK_LCTRL:
        case SDLK_RCTRL:  return Key::Ctrl;
        case SDLK_PLUS:
        case SDLK_KP_PLUS:  return Key::Plus;
        case SDLK_MINUS:
        case SDLK_KP_MINUS: return Key::Minus;
        case SDLK_0:
        case SDLK_KP_0:     return Key::Zero;
        default: return Key::Unknown;
    }
}

MouseButton translate_button(std::uint8_t sdl_button) {
    switch (sdl_button) {
        case SDL_BUTTON_LEFT:   return MouseButton::Left;
        case SDL_BUTTON_RIGHT:  return MouseButton::Right;
        case SDL_BUTTON_MIDDLE: return MouseButton::Middle;
        default: return MouseButton::Left;
    }
}

} // namespace

// Опрос очереди событий SDL2 за один кадр (контракт п. 4.1.2.4: input.poll()).
// Возвращает false, если получено событие SDL_QUIT — главный цикл завершается.
bool poll_sdl_events(InputState& state, Window& window, ui::UiRoot& ui_root) {
    SDL_Event event;
    bool quit = false;

    while (SDL_PollEvent(&event) != 0) {
        switch (event.type) {
            case SDL_QUIT:
                quit = true;
                break;

            case SDL_WINDOWEVENT:
                if (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED ||
                    event.window.event == SDL_WINDOWEVENT_RESIZED) {
                    const auto w = static_cast<std::uint32_t>(event.window.data1);
                    const auto h = static_cast<std::uint32_t>(event.window.data2);
                    const auto viewport = window.on_resized(w, h);
                    ui_root.resize(viewport.window_w, viewport.window_h);
                    core::Log::instance().debug("window: resized " + std::to_string(w) + "x" +
                                                std::to_string(h) + ", scale=" +
                                                std::to_string(viewport.scale));
                }
                break;

            case SDL_KEYDOWN:
            case SDL_KEYUP: {
                const Key key = translate_key(event.key.keysym.sym);
                const bool pressed = (event.type == SDL_KEYDOWN);
                state.set_key(key, pressed);
                InputEvent e;
                e.type = pressed ? InputEvent::Type::KeyDown : InputEvent::Type::KeyUp;
                e.key = key;
                e.x = state.mouse_x();
                e.y = state.mouse_y();
                if (pressed) {
                    // «нажата в этом кадре» — для однократных действий
                    state.push_event(e);
                } else {
                    state.push_event(e);
                }
                break;
            }

            case SDL_MOUSEMOTION: {
                const auto x = static_cast<float>(event.motion.x);
                const auto y = static_cast<float>(event.motion.y);
                state.set_mouse_position(x, y);
                InputEvent e;
                e.type = InputEvent::Type::MouseMove;
                e.x = x;
                e.y = y;
                state.push_event(e);
                break;
            }

            case SDL_MOUSEBUTTONDOWN:
            case SDL_MOUSEBUTTONUP: {
                const MouseButton button = translate_button(event.button.button);
                const bool pressed = (event.type == SDL_MOUSEBUTTONDOWN);
                state.set_mouse_button(button, pressed);
                InputEvent e;
                e.type = pressed ? InputEvent::Type::MouseDown : InputEvent::Type::MouseUp;
                e.button = button;
                e.x = static_cast<float>(event.button.x);
                e.y = static_cast<float>(event.button.y);
                state.set_mouse_position(e.x, e.y);
                state.push_event(e);
                break;
            }

            case SDL_MOUSEWHEEL: {
                InputEvent e;
                e.type = InputEvent::Type::MouseWheel;
                e.wheel_delta = event.wheel.y;     // колесо (FR-E03)
                e.x = state.mouse_x();
                e.y = state.mouse_y();
                state.push_event(e);
                break;
            }

            default:
                break; // прочие события (геймпад, сенсор — V1) не обрабатываются в MVP
        }
    }
    return !quit;
}

} // namespace engine::platform

#endif // DND_ENGINE_WITH_SDL2
