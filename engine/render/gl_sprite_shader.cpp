// engine/render/gl_sprite_shader.cpp — компиляция и линковка шейдерной программы.
#include "render/gl_sprite_shader.h"

#include <vector>

namespace engine::render {

namespace {

constexpr gl::GLsizei kLogBufferSize = 1024;

bool compile_stage(const gl::GlFunctions& fns, gl::GLenum type, const char* source,
                   gl::GLuint& out_shader, std::string* reason) {
    out_shader = fns.glCreateShader_(type);
    if (out_shader == 0) {
        if (reason != nullptr) *reason = "glCreateShader вернул 0";
        return false;
    }
    fns.glShaderSource_(out_shader, 1, &source, nullptr);
    fns.glCompileShader_(out_shader);

    gl::GLint status = 0;
    fns.glGetShaderiv_(out_shader, gl::GL_COMPILE_STATUS, &status);
    if (status == 0) {
        std::vector<gl::GLchar> buffer(kLogBufferSize, '\0');
        fns.glGetShaderInfoLog_(out_shader, kLogBufferSize, nullptr, buffer.data());
        if (reason != nullptr) {
            *reason = std::string(type == gl::GL_VERTEX_SHADER ? "vertex: " : "fragment: ") +
                      buffer.data();
        }
        fns.glDeleteShader_(out_shader);
        out_shader = 0;
        return false;
    }
    return true;
}

} // namespace

GlSpriteShader::~GlSpriteShader() { destroy(); }

bool GlSpriteShader::compile(const gl::GlFunctions& fns, const char* vertex_src,
                             const char* fragment_src, std::string* reason) {
    destroy();
    fns_ = &fns;

    gl::GLuint vs = 0;
    gl::GLuint fs = 0;
    if (!compile_stage(fns, gl::GL_VERTEX_SHADER, vertex_src, vs, reason)) return false;
    if (!compile_stage(fns, gl::GL_FRAGMENT_SHADER, fragment_src, fs, reason)) {
        fns.glDeleteShader_(vs);
        return false;
    }

    program_ = fns.glCreateProgram_();
    if (program_ == 0) {
        fns.glDeleteShader_(vs);
        fns.glDeleteShader_(fs);
        if (reason != nullptr) *reason = "glCreateProgram вернул 0";
        return false;
    }
    fns.glAttachShader_(program_, vs);
    fns.glAttachShader_(program_, fs);
    fns.glLinkProgram_(program_);

    gl::GLint status = 0;
    fns.glGetProgramiv_(program_, gl::GL_LINK_STATUS, &status);
    if (status == 0) {
        std::vector<gl::GLchar> buffer(kLogBufferSize, '\0');
        fns.glGetProgramInfoLog_(program_, kLogBufferSize, nullptr, buffer.data());
        if (reason != nullptr) *reason = std::string("link: ") + buffer.data();
        fns.glDetachShader_(program_, vs);
        fns.glDetachShader_(program_, fs);
        fns.glDeleteProgram_(program_);
        program_ = 0;
        fns.glDeleteShader_(vs);
        fns.glDeleteShader_(fs);
        return false;
    }

    // Шейдерные объекты больше не нужны: программа хранит свою копию
    fns.glDetachShader_(program_, vs);
    fns.glDetachShader_(program_, fs);
    fns.glDeleteShader_(vs);
    fns.glDeleteShader_(fs);

    u_proj_ = fns.glGetUniformLocation_(program_, "u_proj");
    u_tex_ = fns.glGetUniformLocation_(program_, "u_tex");
    return true;
}

void GlSpriteShader::bind() const {
    if (fns_ != nullptr && program_ != 0) fns_->glUseProgram_(program_);
}

void GlSpriteShader::release() const {
    if (fns_ != nullptr) fns_->glUseProgram_(0);
}

void GlSpriteShader::destroy() {
    if (fns_ != nullptr && program_ != 0) {
        fns_->glDeleteProgram_(program_);
    }
    program_ = 0;
    u_proj_ = -1;
    u_tex_ = -1;
}

void GlSpriteShader::set_projection(const float m16[16]) const {
    if (fns_ != nullptr && u_proj_ >= 0) {
        fns_->glUniformMatrix4fv_(u_proj_, 1, gl::GL_FALSE_VALUE, m16);
    }
}

} // namespace engine::render
