// engine/render/gl_backend.cpp — реализация OpenGL 3.3 Core бэкенда (FR-E20…FR-E32).
// Функции GL загружаются динамически (render/gl_functions.h), поэтому libGL не
// участвует в линковке: headless-сборка и тесты работают без графического драйвера.
#include "render/gl_renderer.h"

#ifdef DND_ENGINE_WITH_SDL2

#include <SDL.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <cstring>

#if defined(__unix__) || defined(__APPLE__)
#include <dlfcn.h>
#endif

#include "core/log.h"

namespace engine::render {

namespace {

constexpr gl::GLenum kGlVersion = 0x1F02;  // GL_VERSION
constexpr gl::GLenum kGlRenderer = 0x1F01; // GL_RENDERER
constexpr gl::GLenum kGlVendor = 0x1F00;   // GL_VENDOR

// Число вершин на квад и размер вершины в байтах
constexpr std::size_t kVerticesPerQuad = 4;
constexpr std::size_t kIndicesPerQuad = 6;

// Загрузка адресов функций: SDL_GL_GetProcAddress, при недоступности — dlopen libGL
void* (*g_sdl_loader)(const char*) = nullptr;
void* g_gl_handle = nullptr;

void* resolve_gl_symbol(const char* name) {
    if (g_sdl_loader != nullptr) {
        void* p = g_sdl_loader(name);
        if (p != nullptr) return p;
    }
#if defined(__unix__)
    if (g_gl_handle == nullptr) {
        static const char* candidates[] = {"libGL.so.1", "libGL.so", "libEGL.so.1"};
        for (const char* lib : candidates) {
            g_gl_handle = dlopen(lib, RTLD_NOW | RTLD_LOCAL);
            if (g_gl_handle != nullptr) break;
        }
    }
    if (g_gl_handle != nullptr) return dlsym(g_gl_handle, name);
#endif
    return nullptr;
}

const char* gl_string(const gl::GlFunctions& gl, gl::GLenum name) {
    if (gl.glGetString_ == nullptr) return "";
    const auto* value = gl.glGetString_(name);
    return value != nullptr ? reinterpret_cast<const char*>(value) : "";
}

} // namespace

// --- жизненный цикл ---------------------------------------------------------

GlRenderer::GlRenderer(std::size_t quad_capacity) : quad_capacity_(quad_capacity) {
    if (quad_capacity_ < 16) quad_capacity_ = 16;
    vertices_.resize(quad_capacity_ * kVerticesPerQuad);
    indices_.resize(quad_capacity_ * kIndicesPerQuad);
}

GlRenderer::~GlRenderer() { shutdown(); }

bool GlRenderer::init(std::uint32_t logical_width, std::uint32_t logical_height,
                      void* (*loader)(const char*), GlInitInfo* info) {
    if (ready_) return true;
    logical_width_ = logical_width;
    logical_height_ = logical_height;
    window_width_ = logical_width;
    window_height_ = logical_height;

    g_sdl_loader = (loader != nullptr) ? loader : &SDL_GL_GetProcAddress;

    std::string missing;
    if (!gl::load_gl_functions(gl_, &resolve_gl_symbol, &missing)) {
        if (info != nullptr) info->error = "нет функции GL: " + missing;
        core::Log::instance().error("render: OpenGL недоступен (" + missing +
                                    ") — используется NullRenderer");
        return false;
    }
    if (!gl::has_gl_base(gl_)) {
        if (info != nullptr) info->error = "неполный набор функций OpenGL";
        return false;
    }

    std::string reason;
    if (!shader_.compile(gl_, kSpriteVertexSource, kSpriteFragmentSource, &reason)) {
        if (info != nullptr) info->error = "шейдер: " + reason;
        core::Log::instance().error("render: компиляция шейдера не удалась: " + reason);
        return false;
    }

    gl_.glGenVertexArrays_(1, &vao_);
    gl_.glGenBuffers_(1, &vbo_);
    gl_.glGenBuffers_(1, &ebo_);
    if (vao_ == 0 || vbo_ == 0 || ebo_ == 0) {
        if (info != nullptr) info->error = "не удалось создать буферы GL";
        shader_.destroy();
        return false;
    }

    gl_.glBindVertexArray_(vao_);
    gl_.glBindBuffer_(gl::GL_ARRAY_BUFFER, vbo_);
    gl_.glBufferData_(gl::GL_ARRAY_BUFFER,
                      static_cast<gl::GLsizeiptr>(vertices_.size() * sizeof(Vertex)), nullptr,
                      gl::GL_DYNAMIC_DRAW);
    gl_.glBindBuffer_(gl::GL_ELEMENT_ARRAY_BUFFER, ebo_);
    gl_.glBufferData_(gl::GL_ELEMENT_ARRAY_BUFFER,
                      static_cast<gl::GLsizeiptr>(indices_.size() * sizeof(std::uint16_t)), nullptr,
                      gl::GL_DYNAMIC_DRAW);

    const int stride = static_cast<int>(sizeof(Vertex));
    const auto offset = [](std::size_t member) {
        return reinterpret_cast<const gl::GLvoid*>(member);
    };
    gl_.glEnableVertexAttribArray_(0);
    gl_.glVertexAttribPointer_(0, 2, gl::GL_FLOAT, gl::GL_FALSE_VALUE, stride, offset(offsetof(Vertex, x)));
    gl_.glEnableVertexAttribArray_(1);
    gl_.glVertexAttribPointer_(1, 2, gl::GL_FLOAT, gl::GL_FALSE_VALUE, stride, offset(offsetof(Vertex, u)));
    gl_.glEnableVertexAttribArray_(2);
    gl_.glVertexAttribPointer_(2, 4, gl::GL_FLOAT, gl::GL_FALSE_VALUE, stride, offset(offsetof(Vertex, r)));
    gl_.glEnableVertexAttribArray_(3);
    gl_.glVertexAttribPointer_(3, 1, gl::GL_FLOAT, gl::GL_FALSE_VALUE, stride, offset(offsetof(Vertex, radius)));
    gl_.glEnableVertexAttribArray_(4);
    gl_.glVertexAttribPointer_(4, 2, gl::GL_FLOAT, gl::GL_FALSE_VALUE, stride, offset(offsetof(Vertex, cx)));
    gl_.glEnableVertexAttribArray_(5);
    gl_.glVertexAttribPointer_(5, 2, gl::GL_FLOAT, gl::GL_FALSE_VALUE, stride, offset(offsetof(Vertex, hw)));

    gl_.glEnable_(gl::GL_BLEND);
    gl_.glBlendFunc_(gl::GL_SRC_ALPHA, gl::GL_ONE_MINUS_SRC_ALPHA);
    gl_.glDisable_(0x0B71); // GL_DEPTH_TEST: 2D-отрисовка без буфера глубины
    // Двойная буферизация: рисуем в задний буфер, читаем его же для снимка экрана
    if (gl_.glDrawBuffer_ != nullptr) gl_.glDrawBuffer_(gl::GL_BACK);
    if (gl_.glReadBuffer_ != nullptr) gl_.glReadBuffer_(gl::GL_BACK);

    // Готовность выставляется до создания служебных текстур: create_texture_rgba
    // отклоняет вызовы неинициализированного бэкенда (дефект обнаружен CI-прогоном)
    ready_ = true;

    // Однотонная белая текстура 1×1 для примитивов и тонирования
    const std::uint8_t white[4] = {255, 255, 255, 255};
    white_texture_ = create_texture_rgba(1, 1, white);
    if (white_texture_ == kInvalidTexture) {
        core::Log::instance().error("render: не создана служебная текстура примитивов");
    }

    // Атлас встроенного шрифта (FR-E29)
    if (font_.bake()) {
        font_texture_ = create_texture_rgba(static_cast<std::uint32_t>(font_.width()),
                                            static_cast<std::uint32_t>(font_.height()),
                                            font_.pixels().data());
        if (font_texture_ == kInvalidTexture) {
            core::Log::instance().error("render: атлас шрифта запечён, но текстура не создана");
        }
    } else {
        core::Log::instance().error("render: не удалось запечь шрифтовой атлас — текст недоступен");
    }

    resize(window_width_, window_height_);

    if (info != nullptr) {
        info->version = gl_string(gl_, kGlVersion);
        info->renderer = gl_string(gl_, kGlRenderer);
        info->vendor = gl_string(gl_, kGlVendor);
        info->error.clear();
    }
    core::Log::instance().info(std::string("render: OpenGL ") + gl_string(gl_, kGlVersion) +
                               " (" + gl_string(gl_, kGlRenderer) + ")");
    return true;
}

void GlRenderer::destroy_batch_resources() {
    if (!ready_) return;
    for (const auto& [id, name] : textures_) {
        (void)id;
        gl_.glDeleteTextures_(1, &name);
    }
    textures_.clear();
    texture_sizes_.clear();
    if (vbo_ != 0) gl_.glDeleteBuffers_(1, &vbo_);
    if (ebo_ != 0) gl_.glDeleteBuffers_(1, &ebo_);
    if (vao_ != 0) gl_.glDeleteVertexArrays_(1, &vao_);
    vbo_ = ebo_ = vao_ = 0;
}

void GlRenderer::shutdown() {
    if (!ready_) return;
    flush();
    destroy_batch_resources();
    shader_.destroy();
    ready_ = false;
    font_texture_ = kInvalidTexture;
    white_texture_ = kInvalidTexture;
}

void GlRenderer::resize(std::uint32_t window_width, std::uint32_t window_height) {
    window_width_ = window_width != 0 ? window_width : 1;
    window_height_ = window_height != 0 ? window_height : 1;
    if (!ready_) return;
    gl_.glViewport_(0, 0, static_cast<gl::GLint>(window_width_),
                    static_cast<gl::GLint>(window_height_));
    apply_projection();
    set_scissor_state();
}

void GlRenderer::apply_projection() {
    // Ортографическая проекция логического пространства 0..W, 0..H (ось Y вниз)
    const float w = static_cast<float>(logical_width_);
    const float h = static_cast<float>(logical_height_);
    float m[16] = {0.f};
    m[0] = 2.f / w;
    m[5] = -2.f / h;
    m[10] = -1.f;
    m[12] = -1.f;
    m[13] = 1.f;
    m[15] = 1.f;
    shader_.bind();
    shader_.set_projection(m);
}

// --- текстуры ---------------------------------------------------------------

TextureId GlRenderer::create_texture_rgba(std::uint32_t width, std::uint32_t height,
                                          const std::uint8_t* pixels) {
    if (!ready_ || width == 0 || height == 0 || pixels == nullptr) return kInvalidTexture;

    unsigned name = 0;
    gl_.glGenTextures_(1, &name);
    if (name == 0) {
        core::Log::instance().error("render: glGenTextures вернул 0");
        return kInvalidTexture;
    }
    gl_.glBindTexture_(gl::GL_TEXTURE_2D, name);
    gl_.glTexImage2D_(gl::GL_TEXTURE_2D, 0, static_cast<gl::GLint>(gl::GL_RGBA8),
                      static_cast<gl::GLsizei>(width), static_cast<gl::GLsizei>(height), 0,
                      gl::GL_RGBA, gl::GL_UNSIGNED_BYTE, pixels);
    // Линейная фильтрация: текст и спрайты масштабируются (FR-E02, формула (1))
    gl_.glTexParameteri_(gl::GL_TEXTURE_2D, gl::GL_TEXTURE_MIN_FILTER,
                         static_cast<gl::GLint>(gl::GL_LINEAR));
    gl_.glTexParameteri_(gl::GL_TEXTURE_2D, gl::GL_TEXTURE_MAG_FILTER,
                         static_cast<gl::GLint>(gl::GL_LINEAR));
    gl_.glTexParameteri_(gl::GL_TEXTURE_2D, gl::GL_TEXTURE_WRAP_S,
                         static_cast<gl::GLint>(gl::GL_CLAMP_TO_EDGE));
    gl_.glTexParameteri_(gl::GL_TEXTURE_2D, gl::GL_TEXTURE_WRAP_T,
                         static_cast<gl::GLint>(gl::GL_CLAMP_TO_EDGE));

    const TextureId id = next_texture_id_++;
    textures_.emplace_back(id, name);
    texture_sizes_.emplace_back(id, std::make_pair(width, height));
    return id;
}

TextureId GlRenderer::create_solid_texture(std::uint32_t width, std::uint32_t height,
                                           const Color& color) {
    std::vector<std::uint8_t> pixels(static_cast<std::size_t>(width) * height * 4);
    for (std::size_t i = 0; i + 3 < pixels.size(); i += 4) {
        pixels[i + 0] = static_cast<std::uint8_t>(std::clamp(color.r, 0.f, 1.f) * 255.f);
        pixels[i + 1] = static_cast<std::uint8_t>(std::clamp(color.g, 0.f, 1.f) * 255.f);
        pixels[i + 2] = static_cast<std::uint8_t>(std::clamp(color.b, 0.f, 1.f) * 255.f);
        pixels[i + 3] = static_cast<std::uint8_t>(std::clamp(color.a, 0.f, 1.f) * 255.f);
    }
    return create_texture_rgba(width, height, pixels.data());
}

void GlRenderer::destroy_texture(TextureId id) {
    if (id == kInvalidTexture) return;
    for (auto it = textures_.begin(); it != textures_.end(); ++it) {
        if (it->first == id) {
            if (ready_) gl_.glDeleteTextures_(1, &it->second);
            textures_.erase(it);
            break;
        }
    }
    for (auto it = texture_sizes_.begin(); it != texture_sizes_.end(); ++it) {
        if (it->first == id) {
            texture_sizes_.erase(it);
            break;
        }
    }
    if (id == font_texture_) font_texture_ = kInvalidTexture;
    if (id == white_texture_) white_texture_ = kInvalidTexture;
}

std::size_t GlRenderer::texture_count() const { return textures_.size(); }

namespace {

unsigned gl_name_of(const std::vector<std::pair<TextureId, unsigned>>& textures, TextureId id,
                    unsigned fallback) {
    for (const auto& [texture_id, name] : textures) {
        if (texture_id == id) return name;
    }
    return fallback;
}

std::pair<std::uint32_t, std::uint32_t> texture_size(
    const std::vector<std::pair<TextureId, std::pair<std::uint32_t, std::uint32_t>>>& sizes,
    TextureId id) {
    for (const auto& [texture_id, size] : sizes) {
        if (texture_id == id) return size;
    }
    return {1, 1};
}

} // namespace

// --- пакетирование ----------------------------------------------------------

void GlRenderer::emit_quad(TextureId texture, float x0, float y0, float x1, float y1, float u0,
                           float v0, float u1, float v1, const Color& tint, float radius) {
    if (!ready_) return;
    // Смена текстуры или типа примитива закрывает пакет: один draw call на текстуру
    // (E-D05, NFR-E01)
    if (vertex_count_ > 0 && (texture != batch_texture_ || batch_is_triangles_)) flush();
    if (vertex_count_ + kVerticesPerQuad > quad_capacity_ * kVerticesPerQuad) flush();

    batch_texture_ = texture;
    batch_is_triangles_ = false;
    const float cx = (x0 + x1) * 0.5f;
    const float cy = (y0 + y1) * 0.5f;
    const float hw = std::abs(x1 - x0) * 0.5f;
    const float hh = std::abs(y1 - y0) * 0.5f;

    Vertex v[4];
    v[0] = {x0, y0, u0, v0, tint.r, tint.g, tint.b, tint.a, radius, cx, cy, hw, hh};
    v[1] = {x1, y0, u1, v0, tint.r, tint.g, tint.b, tint.a, radius, cx, cy, hw, hh};
    v[2] = {x1, y1, u1, v1, tint.r, tint.g, tint.b, tint.a, radius, cx, cy, hw, hh};
    v[3] = {x0, y1, u0, v1, tint.r, tint.g, tint.b, tint.a, radius, cx, cy, hw, hh};

    for (std::size_t i = 0; i < 4; ++i) vertices_[vertex_count_ + i] = v[i];
    vertex_count_ += 4;
    ++sprites_drawn_;
}

void GlRenderer::flush() {
    if (!ready_) {
        vertex_count_ = 0;
        return;
    }
    if (vertex_count_ == 0) return;

    // Пакет однороден по типу примитива: emit_quad/emit_triangle закрывают пакет
    // при смене типа, поэтому здесь достаточно одного ветвления (E-D05).
    const std::size_t quad_count =
        batch_is_triangles_ ? 0 : vertex_count_ / kVerticesPerQuad;
    for (std::size_t q = 0; q < quad_count; ++q) {
        const auto base = static_cast<std::uint16_t>(q * kVerticesPerQuad);
        const std::size_t i = q * kIndicesPerQuad;
        indices_[i + 0] = static_cast<std::uint16_t>(base + 0);
        indices_[i + 1] = static_cast<std::uint16_t>(base + 1);
        indices_[i + 2] = static_cast<std::uint16_t>(base + 2);
        indices_[i + 3] = static_cast<std::uint16_t>(base + 2);
        indices_[i + 4] = static_cast<std::uint16_t>(base + 3);
        indices_[i + 5] = static_cast<std::uint16_t>(base + 0);
    }

    shader_.bind();
    gl_.glBindVertexArray_(vao_);
    gl_.glBindBuffer_(gl::GL_ARRAY_BUFFER, vbo_);
    gl_.glBufferSubData_(gl::GL_ARRAY_BUFFER, 0,
                         static_cast<gl::GLsizeiptr>(vertex_count_ * sizeof(Vertex)),
                         vertices_.data());
    if (quad_count > 0) {
        gl_.glBindBuffer_(gl::GL_ELEMENT_ARRAY_BUFFER, ebo_);
        gl_.glBufferSubData_(gl::GL_ELEMENT_ARRAY_BUFFER, 0,
                             static_cast<gl::GLsizeiptr>(quad_count * kIndicesPerQuad *
                                                         sizeof(std::uint16_t)),
                             indices_.data());
    }

    const unsigned name =
        gl_name_of(textures_, batch_texture_,
                   white_texture_ != kInvalidTexture
                       ? gl_name_of(textures_, white_texture_, 0)
                       : 0);
    gl_.glActiveTexture_(gl::GL_TEXTURE0);
    gl_.glBindTexture_(gl::GL_TEXTURE_2D, name);
    if (shader_.uniform_tex() >= 0) gl_.glUniform1i_(shader_.uniform_tex(), 0);

    set_scissor_state();
    if (quad_count > 0) {
        gl_.glDrawElements_(gl::GL_TRIANGLES,
                            static_cast<gl::GLsizei>(quad_count * kIndicesPerQuad),
                            gl::GL_UNSIGNED_SHORT, nullptr);
        ++draw_calls_;
    } else if (vertex_count_ > 0) {
        gl_.glDrawArrays_(gl::GL_TRIANGLES, 0, static_cast<gl::GLsizei>(vertex_count_));
        ++draw_calls_;
    }
    ++flush_count_;
    if (gl_.glGetError_ != nullptr) {
        const gl::GLenum error = gl_.glGetError_();
        if (error != gl::GL_NO_ERROR) {
            last_gl_error_ = error;
            core::Log::instance().error("render: ошибка GL 0x" +
                                        std::to_string(static_cast<unsigned>(error)));
        }
    }

    vertex_count_ = 0;
    batch_texture_ = kInvalidTexture;
    batch_is_triangles_ = false;
}

void GlRenderer::begin_frame() {
    draw_calls_ = 0;
    flush_count_ = 0;
    sprites_drawn_ = 0;
    text_glyphs_drawn_ = 0;
    vertex_count_ = 0;
    batch_is_triangles_ = false;
    scissor_stack_.clear();
    if (!ready_) return;
    gl_.glClearColor_(0.09f, 0.11f, 0.15f, 1.f); // фон кадра; поля letterbox — чёрные (FR-E02)
    gl_.glClear_(gl::GL_COLOR_BUFFER_BIT);
    gl_.glViewport_(0, 0, static_cast<gl::GLint>(window_width_),
                    static_cast<gl::GLint>(window_height_));
    apply_projection();
}

void GlRenderer::end_frame() { flush(); }

// --- отрисовка --------------------------------------------------------------

void GlRenderer::draw_sprite(TextureId texture, const Rect& src, const Rect& dst,
                             const Color& tint) {
    draw_sprite_flipped(texture, src, dst, tint, false, false);
}

void GlRenderer::draw_sprite_flipped(TextureId texture, const Rect& src, const Rect& dst,
                                     const Color& tint, bool flip_x, bool flip_y) {
    if (!ready_ || texture == kInvalidTexture) return;
    const auto [tw, th] = texture_size(texture_sizes_, texture);
    const float inv_w = 1.f / static_cast<float>(tw != 0 ? tw : 1);
    const float inv_h = 1.f / static_cast<float>(th != 0 ? th : 1);
    float u0 = src.x * inv_w;
    float v0 = src.y * inv_h;
    float u1 = (src.x + src.w) * inv_w;
    float v1 = (src.y + src.h) * inv_h;
    if (flip_x) std::swap(u0, u1); // зеркальный разворот (FR-E31)
    if (flip_y) std::swap(v0, v1);
    emit_quad(texture, dst.x, dst.y, dst.x + dst.w, dst.y + dst.h, u0, v0, u1, v1, tint, 0.f);
}

void GlRenderer::draw_rect(const Rect& rect, const Color& color) {
    if (!ready_) return;
    emit_quad(white_texture_, rect.x, rect.y, rect.x + rect.w, rect.y + rect.h, 0.f, 0.f, 1.f,
              1.f, color, 0.f);
}

void GlRenderer::draw_rect_outline(const Rect& rect, const Color& color, float thickness) {
    if (!ready_) return;
    const float t = std::max(0.5f, thickness);
    draw_rect({rect.x, rect.y, rect.w, t}, color);                              // верх
    draw_rect({rect.x, rect.y + rect.h - t, rect.w, t}, color);                 // низ
    draw_rect({rect.x, rect.y + t, t, rect.h - 2.f * t}, color);                // лево
    draw_rect({rect.x + rect.w - t, rect.y + t, t, rect.h - 2.f * t}, color);    // право
}

void GlRenderer::emit_triangle(TextureId texture, const Vertex& a, const Vertex& b,
                               const Vertex& c) {
    if (!ready_) return;
    if (vertex_count_ > 0 && (texture != batch_texture_ || !batch_is_triangles_)) flush();
    if (vertex_count_ + 3 > quad_capacity_ * kVerticesPerQuad) flush();
    batch_texture_ = texture;
    batch_is_triangles_ = true;
    vertices_[vertex_count_ + 0] = a;
    vertices_[vertex_count_ + 1] = b;
    vertices_[vertex_count_ + 2] = c;
    vertex_count_ += 3;
}

void GlRenderer::draw_line(Vec2f from, Vec2f to, const Color& color, float thickness) {
    if (!ready_) return;
    const float dx = to.x - from.x;
    const float dy = to.y - from.y;
    const float length = std::sqrt(dx * dx + dy * dy);
    if (length < 1e-4f) {
        draw_circle(from, std::max(0.5f, thickness * 0.5f), color);
        return;
    }
    // Ориентированный прямоугольник из двух треугольников: линия под любым углом
    const float t = std::max(0.5f, thickness) * 0.5f;
    const float nx = -dy / length * t;
    const float ny = dx / length * t;
    const Vertex a{from.x + nx, from.y + ny, 0.f, 0.f, color.r, color.g, color.b, color.a,
                   0.f, from.x, from.y, 1.f, 1.f};
    const Vertex b{to.x + nx, to.y + ny, 0.f, 0.f, color.r, color.g, color.b, color.a,
                   0.f, to.x, to.y, 1.f, 1.f};
    const Vertex c{to.x - nx, to.y - ny, 0.f, 0.f, color.r, color.g, color.b, color.a,
                   0.f, to.x, to.y, 1.f, 1.f};
    const Vertex d{from.x - nx, from.y - ny, 0.f, 0.f, color.r, color.g, color.b, color.a,
                   0.f, from.x, from.y, 1.f, 1.f};
    emit_triangle(white_texture_, a, b, c);
    emit_triangle(white_texture_, a, c, d);
}

void GlRenderer::draw_circle(Vec2f center, float radius, const Color& color) {
    if (!ready_ || radius <= 0.f) return;
    // Режим круга: шейдер вычисляет альфу по расстоянию от центра (антиалиасинг)
    emit_quad(white_texture_, center.x - radius, center.y - radius, center.x + radius,
              center.y + radius, 0.f, 0.f, 1.f, 1.f, color, radius);
}

void GlRenderer::draw_text(float x, float y, const std::string& text, float size,
                           const Color& color) {
    if (!ready_ || font_texture_ == kInvalidTexture || size <= 0.f) return;

    // Кегль задаёт высоту прописных знаков; ячейка глифа выше (диакритика, выносные)
    const float scale = size / FontAtlas::kBaseCapHeight;
    const float cell_h = static_cast<float>(FontAtlas::kCellHeight) * scale;
    const float cell_w = static_cast<float>(FontAtlas::kCellWidth) * scale;
    // Базовая линия: y — верх строки в логических координатах (контракт UI)
    const float top = y;

    float pen_x = x;
    const char* p = text.data();
    std::size_t remaining = text.size();
    while (remaining > 0) {
        std::uint32_t cp = 0;
        const std::size_t n = FontAtlas::decode_utf8(p, remaining, cp);
        if (n == 0) {
            ++p;
            --remaining;
            continue;
        }
        const GlyphRect* g = font_.glyph(cp);
        if (g == nullptr) g = &font_.fallback_glyph(); // подстановка «?» (п. 4.2.1)
        if (cp != ' ') {
            emit_quad(font_texture_, pen_x, top, pen_x + cell_w, top + cell_h, g->u0, g->v0,
                      g->u1, g->v1, color, 0.f);
            ++text_glyphs_drawn_;
        }
        pen_x += g->advance * size;
        p += n;
        remaining -= n;
    }
}

// --- снимок экрана (FR-E103) -------------------------------------------------

bool GlRenderer::save_screenshot_bmp(const std::string& path) {
    if (!ready_ || path.empty()) return false;
    flush();

    const int w = static_cast<int>(window_width_);
    const int h = static_cast<int>(window_height_);
    if (w <= 0 || h <= 0) return false;

    std::vector<std::uint8_t> pixels(static_cast<std::size_t>(w) * static_cast<std::size_t>(h) * 3,
                                     0);
    gl_.glPixelStorei_(gl::GL_PACK_ALIGNMENT, 1);
    if (gl_.glReadBuffer_ != nullptr) gl_.glReadBuffer_(gl::GL_BACK);
    gl_.glReadPixels_(0, 0, static_cast<gl::GLsizei>(w), static_cast<gl::GLsizei>(h), gl::GL_RGB,
                      gl::GL_UNSIGNED_BYTE, pixels.data());

    // BMP: строки снизу вверх, порядок байтов BGR, выравнивание строк до 4 байт
    const int row_stride = ((w * 3 + 3) / 4) * 4;
    const std::uint32_t data_size = static_cast<std::uint32_t>(row_stride) *
                                    static_cast<std::uint32_t>(h);
    std::vector<std::uint8_t> file;
    file.reserve(54 + data_size);
    const auto put_u16 = [&](std::uint16_t v) {
        file.push_back(static_cast<std::uint8_t>(v & 0xFF));
        file.push_back(static_cast<std::uint8_t>((v >> 8) & 0xFF));
    };
    const auto put_u32 = [&](std::uint32_t v) {
        for (int i = 0; i < 4; ++i) file.push_back(static_cast<std::uint8_t>((v >> (8 * i)) & 0xFF));
    };
    file.push_back('B');
    file.push_back('M');
    put_u32(54u + data_size);
    put_u32(0u);
    put_u32(54u);
    put_u32(40u);                                   // размер заголовка BITMAPINFOHEADER
    put_u32(static_cast<std::uint32_t>(w));
    put_u32(static_cast<std::uint32_t>(h));
    put_u16(1u);                                    // плоскостей
    put_u16(24u);                                   // бит на пиксель
    put_u32(0u);                                    // без сжатия
    put_u32(data_size);
    put_u32(2835u);                                 // 72 dpi по X
    put_u32(2835u);                                 // 72 dpi по Y
    put_u32(0u);
    put_u32(0u);

    // glReadPixels возвращает строки снизу вверх (y = 0 — нижний край кадра),
    // формат BMP с положительной высотой хранит их в том же порядке: строку кадра
    // y = 0 записываем первой. Дополнительный переворот НЕ выполняется — иначе
    // снимок оказывается перевёрнутым (дефект обнаружен CI-прогоном оконного режима).
    std::vector<std::uint8_t> row(static_cast<std::size_t>(row_stride), 0);
    for (int y = 0; y < h; ++y) {
        const int src_y = y;
        std::fill(row.begin(), row.end(), static_cast<std::uint8_t>(0));
        for (int x = 0; x < w; ++x) {
            const std::size_t src = (static_cast<std::size_t>(src_y) * static_cast<std::size_t>(w) +
                                     static_cast<std::size_t>(x)) * 3;
            row[static_cast<std::size_t>(x) * 3 + 0] = pixels[src + 2]; // B
            row[static_cast<std::size_t>(x) * 3 + 1] = pixels[src + 1]; // G
            row[static_cast<std::size_t>(x) * 3 + 2] = pixels[src + 0]; // R
        }
        file.insert(file.end(), row.begin(), row.end());
    }

    std::FILE* out = std::fopen(path.c_str(), "wb");
    if (out == nullptr) {
        core::Log::instance().error("render: не удалось открыть файл снимка " + path);
        return false;
    }
    const std::size_t written = std::fwrite(file.data(), 1, file.size(), out);
    std::fclose(out);
    if (written != file.size()) {
        core::Log::instance().error("render: снимок экрана записан не полностью");
        return false;
    }
    return true;
}

// --- ножницы ----------------------------------------------------------------

void GlRenderer::push_scissor(const Rect& logical_rect) {
    scissor_stack_.push_back(logical_rect);
    if (vertex_count_ > 0) flush(); // смена области отсечения закрывает пакет
    set_scissor_state();
}

void GlRenderer::pop_scissor() {
    if (scissor_stack_.empty()) return;
    if (vertex_count_ > 0) flush();
    scissor_stack_.pop_back();
    set_scissor_state();
}

void GlRenderer::set_scissor_state() {
    if (!ready_) return;
    if (scissor_stack_.empty()) {
        gl_.glDisable_(gl::GL_SCISSOR_TEST);
        return;
    }
    // Пересечение всех уровней стека (вложенные панели, п. 4.1.7.4)
    Rect r = scissor_stack_.front();
    for (std::size_t i = 1; i < scissor_stack_.size(); ++i) {
        const Rect& s = scissor_stack_[i];
        const float x0 = std::max(r.x, s.x);
        const float y0 = std::max(r.y, s.y);
        const float x1 = std::min(r.x + r.w, s.x + s.w);
        const float y1 = std::min(r.y + r.h, s.y + s.h);
        r = {x0, y0, std::max(0.f, x1 - x0), std::max(0.f, y1 - y0)};
    }
    // Логические координаты → пиксели окна (формула (1): масштаб S)
    const float sx = static_cast<float>(window_width_) / static_cast<float>(logical_width_);
    const float sy = static_cast<float>(window_height_) / static_cast<float>(logical_height_);
    int x = static_cast<int>(std::floor(r.x * sx));
    int y = static_cast<int>(std::floor(r.y * sy));
    int w = static_cast<int>(std::ceil(r.w * sx));
    int h = static_cast<int>(std::ceil(r.h * sy));
    // OpenGL считает Y от нижнего края окна
    y = static_cast<int>(window_height_) - (y + h);
    x = std::clamp(x, 0, static_cast<int>(window_width_));
    y = std::clamp(y, 0, static_cast<int>(window_height_));
    w = std::clamp(w, 0, static_cast<int>(window_width_) - x);
    h = std::clamp(h, 0, static_cast<int>(window_height_) - y);
    gl_.glEnable_(gl::GL_SCISSOR_TEST);
    gl_.glScissor_(x, y, w, h);
}

} // namespace engine::render

#endif // DND_ENGINE_WITH_SDL2
