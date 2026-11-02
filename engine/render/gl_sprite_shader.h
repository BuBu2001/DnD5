// engine/render/gl_sprite_shader.h — шейдерная программа спрайтового пакета (FR-E22, E-D05).
// Вершинный формат: position(2f) | uv(2f) | color(4f) | radius(1f).
// radius > 0 включает режим круга: альфа считается по расстоянию от центра квада
// (антиалиасинг без отдельного draw call на примитив). Шейдеры — GLSL 330 core.
#pragma once

#include <string>

#include "render/gl_functions.h"

namespace engine::render {

// Исходный код вершинного шейдера (GLSL 330 core)
inline constexpr const char* kSpriteVertexSource = R"GLSL(#version 330 core
layout(location = 0) in vec2 a_pos;      // позиция в логических пикселях
layout(location = 1) in vec2 a_uv;       // координаты в атласе (0..1)
layout(location = 2) in vec4 a_color;    // тонирование (FR-E30)
layout(location = 3) in float a_radius;  // 0 — обычный спрайт, >0 — круг (радиус, лог. px)
layout(location = 4) in vec2 a_center;   // центр квада (для круга и локальных координат)
layout(location = 5) in vec2 a_half;     // половинные размеры квада
uniform mat4 u_proj;
out vec2 v_uv;
out vec4 v_color;
out float v_radius;
out vec2 v_local;   // смещение от центра в долях полуразмеров (-1..1)
out vec2 v_half;
void main() {
    v_uv = a_uv;
    v_color = a_color;
    v_radius = a_radius;
    v_half = a_half;
    vec2 d = a_half;
    if (d.x <= 0.0) d.x = 1.0;
    if (d.y <= 0.0) d.y = 1.0;
    v_local = (a_pos - a_center) / d;
    gl_Position = u_proj * vec4(a_pos, 0.0, 1.0);
}
)GLSL";

// Фрагментный шейдер: текстура × тонирование; для кругов — сглаживание по радиусу.
inline constexpr const char* kSpriteFragmentSource = R"GLSL(#version 330 core
in vec2 v_uv;
in vec4 v_color;
in float v_radius;
in vec2 v_local;
in vec2 v_half;
uniform sampler2D u_tex;
out vec4 frag;
void main() {
    if (v_radius > 0.0) {
        float dist_px = length(v_local * v_half);
        float aa = fwidth(dist_px) + 0.0001;
        float alpha = 1.0 - smoothstep(v_radius - aa, v_radius + aa, dist_px);
        frag = v_color * alpha;
    } else {
        frag = texture(u_tex, v_uv) * v_color;
    }
}
)GLSL";

// Обёртка над шейдерной программой: компиляция, линковка, журнал ошибок (п. 4.2.1 —
// ошибки ресурсов пишутся в журнал, работа продолжается).
class GlSpriteShader {
public:
    GlSpriteShader() = default;
    ~GlSpriteShader();

    GlSpriteShader(const GlSpriteShader&) = delete;
    GlSpriteShader& operator=(const GlSpriteShader&) = delete;

    // Компиляция и линковка. При неудаче reason содержит сообщение драйвера,
    // valid() == false, программа не используется (рендерер деградирует в Null).
    bool compile(const gl::GlFunctions& fns, const char* vertex_src, const char* fragment_src,
                 std::string* reason);

    void bind() const;
    void release() const;
    void destroy();

    bool valid() const { return program_ != 0; }
    unsigned program() const { return program_; }
    int uniform_proj() const { return u_proj_; }
    int uniform_tex() const { return u_tex_; }

    // Установка ортографической матрицы 4×4 (column-major, как ожидает GLSL)
    void set_projection(const float m16[16]) const;

private:
    const gl::GlFunctions* fns_ = nullptr;
    unsigned program_ = 0;
    int u_proj_ = -1;
    int u_tex_ = -1;
};

} // namespace engine::render
