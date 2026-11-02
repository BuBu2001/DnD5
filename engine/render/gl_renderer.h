// engine/render/gl_renderer.h — рендер-бэкенд OpenGL 3.3 Core (FR-E20…FR-E32, этап M2).
// Реализует IRenderer: пакетирование спрайтов в один буфер (E-D05), примитивы,
// текст через встроенный растровый атлас (FR-E29). Все функции GL загружаются
// динамически (render/gl_functions.h): libGL не линкуется, поэтому сборка и тесты
// работают в headless-среде без драйвера (E-D11); при отсутствии GL 3.3 Core
// init() возвращает false и вызывающий код использует NullRenderer (п. 4.2.3).
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "render/font_atlas.h"
#include "render/gl_functions.h"
#include "render/gl_sprite_shader.h"
#include "render/renderer.h"

namespace engine::render {

struct GlInitInfo {
    std::string version;    // строка GL_VERSION
    std::string renderer;   // строка GL_RENDERER
    std::string vendor;
    std::string error;      // причина отказа (пустая при успехе)
};

class GlRenderer final : public IRenderer {
public:
    // capacity — число квадов в одном пакете (по умолчанию 4096: при 1280×720 и
    // плитках 32×32 это ~4 кадра карты, NFR-E02)
    explicit GlRenderer(std::size_t quad_capacity = 4096);
    ~GlRenderer() override;

    GlRenderer(const GlRenderer&) = delete;
    GlRenderer& operator=(const GlRenderer&) = delete;

    // Инициализация после создания GL-контекста (platform::Window::create).
    // loader — функция получения адресов (SDL_GL_GetProcAddress).
    bool init(std::uint32_t logical_width, std::uint32_t logical_height,
              void* (*loader)(const char*), GlInitInfo* info = nullptr);
    void shutdown();
    bool ready() const { return ready_; }

    // Изменение размера окна: обновляет glViewport и матрицу проекции (FR-E02)
    void resize(std::uint32_t window_width, std::uint32_t window_height);

    // --- текстуры (FR-E21): RGBA8, загружаются из распакованных данных ---------
    TextureId create_texture_rgba(std::uint32_t width, std::uint32_t height,
                                  const std::uint8_t* pixels);
    TextureId create_solid_texture(std::uint32_t width, std::uint32_t height, const Color& color);
    void destroy_texture(TextureId id);
    std::size_t texture_count() const;

    // --- IRenderer -------------------------------------------------------------
    void draw_sprite(TextureId texture, const Rect& src, const Rect& dst,
                     const Color& tint) override;
    void draw_sprite_flipped(TextureId texture, const Rect& src, const Rect& dst,
                             const Color& tint, bool flip_x, bool flip_y) override;
    void draw_rect(const Rect& rect, const Color& color) override;
    void draw_rect_outline(const Rect& rect, const Color& color, float thickness) override;
    void draw_line(Vec2f from, Vec2f to, const Color& color, float thickness) override;
    void draw_circle(Vec2f center, float radius, const Color& color) override;
    void draw_text(float x, float y, const std::string& text, float size,
                   const Color& color) override;

    void begin_frame() override;
    void flush() override;
    void end_frame() override;
    std::uint32_t draw_call_count() const override { return draw_calls_; }

    // --- ножницы (отсечение UI-панелей, п. 4.1.7.4) ----------------------------
    void push_scissor(const Rect& logical_rect);
    void pop_scissor();

    // Снимок экрана текущего кадра в BMP (24 бита, RGB). Используется отладочной
    // панелью и CI-прогоном оконного режима (FR-E100, FR-E103).
    bool save_screenshot_bmp(const std::string& path);

    // Код последней ошибки GL (0 — ошибок нет): фиксируется после отрисовки кадра
    // и выводится отладочной панелью (FR-E100, п. 4.1.10.2).
    unsigned last_gl_error() const { return last_gl_error_; }

    // Метрики (FR-E100)
    std::size_t queued_vertices() const { return vertex_count_; }
    std::size_t flush_count() const { return flush_count_; }
    std::size_t sprites_drawn() const { return sprites_drawn_; }
    std::size_t text_glyphs_drawn() const { return text_glyphs_drawn_; }
    const FontAtlas& font() const { return font_; }

private:
    struct Vertex {
        float x = 0.f, y = 0.f;
        float u = 0.f, v = 0.f;
        float r = 1.f, g = 1.f, b = 1.f, a = 1.f;
        float radius = 0.f;      // >0 — режим круга
        float cx = 0.f, cy = 0.f; // центр квада
        float hw = 0.f, hh = 0.f; // половинные размеры
    };

    void emit_quad(TextureId texture, float x0, float y0, float x1, float y1, float u0, float v0,
                   float u1, float v1, const Color& tint, float radius);
    void emit_triangle(TextureId texture, const Vertex& a, const Vertex& b, const Vertex& c);
    void set_scissor_state();
    void apply_projection();
    void destroy_batch_resources();

    gl::GlFunctions gl_{};
    GlSpriteShader shader_;
    FontAtlas font_;
    TextureId font_texture_ = kInvalidTexture;
    TextureId white_texture_ = kInvalidTexture;

    unsigned vao_ = 0;
    unsigned vbo_ = 0;
    unsigned ebo_ = 0;
    std::vector<Vertex> vertices_;
    std::vector<std::uint16_t> indices_;
    std::size_t vertex_count_ = 0;
    std::size_t quad_capacity_ = 0;
    TextureId batch_texture_ = kInvalidTexture;
    bool batch_is_triangles_ = false; // тип примитивов текущего пакета (смешивание запрещено)

    std::vector<Rect> scissor_stack_;
    std::uint32_t logical_width_ = 1280;
    std::uint32_t logical_height_ = 720;
    std::uint32_t window_width_ = 1280;
    std::uint32_t window_height_ = 720;

    std::uint32_t draw_calls_ = 0;
    std::size_t flush_count_ = 0;
    std::size_t sprites_drawn_ = 0;
    std::size_t text_glyphs_drawn_ = 0;
    std::uint32_t next_texture_id_ = 1;
    std::vector<std::pair<TextureId, unsigned>> textures_; // id → GL-имя
    std::vector<std::pair<TextureId, std::pair<std::uint32_t, std::uint32_t>>> texture_sizes_;
    bool ready_ = false;
    unsigned last_gl_error_ = 0;
};

} // namespace engine::render
