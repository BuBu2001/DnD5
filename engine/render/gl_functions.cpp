// engine/render/gl_functions.cpp — загрузка указателей функций OpenGL 3.3 Core.
#include "render/gl_functions.h"

namespace engine::render::gl {

namespace {

// Макрос загрузки: при отсутствии символа запоминаем имя и прекращаем загрузку.
#define LOAD_GL(field, name)                                                       \
    out.field = reinterpret_cast<decltype(out.field)>(loader(name));               \
    if (out.field == nullptr) {                                                    \
        if (missing != nullptr) *missing = name;                                   \
        return false;                                                              \
    }

} // namespace

bool load_gl_functions(GlFunctions& out, void* (*loader)(const char*), std::string* missing) {
    out = GlFunctions{};
    if (loader == nullptr) {
        if (missing != nullptr) *missing = "<loader is null>";
        return false;
    }

    LOAD_GL(glEnable_, "glEnable")
    LOAD_GL(glDisable_, "glDisable")
    LOAD_GL(glViewport_, "glViewport")
    LOAD_GL(glScissor_, "glScissor")
    LOAD_GL(glClearColor_, "glClearColor")
    LOAD_GL(glClear_, "glClear")
    LOAD_GL(glBlendFunc_, "glBlendFunc")
    LOAD_GL(glGetError_, "glGetError")
    LOAD_GL(glGetString_, "glGetString")
    LOAD_GL(glGenTextures_, "glGenTextures")
    LOAD_GL(glDeleteTextures_, "glDeleteTextures")
    LOAD_GL(glBindTexture_, "glBindTexture")
    LOAD_GL(glTexImage2D_, "glTexImage2D")
    LOAD_GL(glTexSubImage2D_, "glTexSubImage2D")
    LOAD_GL(glTexParameteri_, "glTexParameteri")
    LOAD_GL(glActiveTexture_, "glActiveTexture")
    LOAD_GL(glGenVertexArrays_, "glGenVertexArrays")
    LOAD_GL(glDeleteVertexArrays_, "glDeleteVertexArrays")
    LOAD_GL(glBindVertexArray_, "glBindVertexArray")
    LOAD_GL(glGenBuffers_, "glGenBuffers")
    LOAD_GL(glDeleteBuffers_, "glDeleteBuffers")
    LOAD_GL(glBindBuffer_, "glBindBuffer")
    LOAD_GL(glBufferData_, "glBufferData")
    LOAD_GL(glBufferSubData_, "glBufferSubData")
    LOAD_GL(glEnableVertexAttribArray_, "glEnableVertexAttribArray")
    LOAD_GL(glVertexAttribPointer_, "glVertexAttribPointer")
    LOAD_GL(glDrawArrays_, "glDrawArrays")
    LOAD_GL(glDrawElements_, "glDrawElements")
    LOAD_GL(glCreateShader_, "glCreateShader")
    LOAD_GL(glDeleteShader_, "glDeleteShader")
    LOAD_GL(glShaderSource_, "glShaderSource")
    LOAD_GL(glCompileShader_, "glCompileShader")
    LOAD_GL(glGetShaderiv_, "glGetShaderiv")
    LOAD_GL(glGetShaderInfoLog_, "glGetShaderInfoLog")
    LOAD_GL(glCreateProgram_, "glCreateProgram")
    LOAD_GL(glDeleteProgram_, "glDeleteProgram")
    LOAD_GL(glAttachShader_, "glAttachShader")
    LOAD_GL(glDetachShader_, "glDetachShader")
    LOAD_GL(glLinkProgram_, "glLinkProgram")
    LOAD_GL(glUseProgram_, "glUseProgram")
    LOAD_GL(glGetProgramiv_, "glGetProgramiv")
    LOAD_GL(glGetProgramInfoLog_, "glGetProgramInfoLog")
    LOAD_GL(glGetUniformLocation_, "glGetUniformLocation")
    LOAD_GL(glUniform1i_, "glUniform1i")
    LOAD_GL(glUniform1f_, "glUniform1f")
    LOAD_GL(glUniform2f_, "glUniform2f")
    LOAD_GL(glUniform4f_, "glUniform4f")
    LOAD_GL(glUniformMatrix4fv_, "glUniformMatrix4fv")
    LOAD_GL(glReadPixels_, "glReadPixels")
    LOAD_GL(glPixelStorei_, "glPixelStorei")
    LOAD_GL(glReadBuffer_, "glReadBuffer")
    LOAD_GL(glDrawBuffer_, "glDrawBuffer")

#undef LOAD_GL
    return true;
}

bool has_gl_base(const GlFunctions& f) {
    return f.glEnable_ != nullptr && f.glViewport_ != nullptr && f.glClear_ != nullptr &&
           f.glGenTextures_ != nullptr && f.glBindTexture_ != nullptr &&
           f.glGenBuffers_ != nullptr && f.glDrawArrays_ != nullptr &&
           f.glCreateProgram_ != nullptr && f.glUniformMatrix4fv_ != nullptr;
}

} // namespace engine::render::gl
