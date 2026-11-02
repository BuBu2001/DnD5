// engine/render/gl_functions.h — загрузчик функций OpenGL 3.3 Core (FR-E20).
// Символы libGL НЕ линкуются статически: указатели получаются через
// SDL_GL_GetProcAddress (или dlopen при проверке). Это позволяет собирать движок
// без системного libGL.so и корректно сообщать об отсутствии драйвера вместо
// аварийного завершения (п. 4.2.3). Заголовок подключается только из .cpp слоя
// отрисовки: правила и ядро его не видят (NFR-E10).
#pragma once

#include <cstddef>
#include <string>

namespace engine::render::gl {

// --- типы OpenGL (аналоги GL-типов, без включения gl/GL.h) ------------------
using GLenum = unsigned int;
using GLboolean = unsigned char;
using GLbitfield = unsigned int;
using GLvoid = void;
using GLubyte = unsigned char;
using GLint = int;
using GLuint = unsigned int;
using GLsizei = int;
using GLfloat = float;
using GLchar = char;
using GLintptr = std::ptrdiff_t;
using GLsizeiptr = std::ptrdiff_t;

// --- константы (значения из спецификации OpenGL 3.3 Core) -------------------
inline constexpr GLenum GL_FALSE_VALUE   = 0;
inline constexpr GLenum GL_TRUE_VALUE    = 1;
inline constexpr GLenum GL_TRIANGLES     = 0x0004;
inline constexpr GLenum GL_BLEND         = 0x0BE2;
inline constexpr GLenum GL_SCISSOR_TEST  = 0x0C11;
inline constexpr GLenum GL_SRC_ALPHA     = 0x0302;
inline constexpr GLenum GL_ONE_MINUS_SRC_ALPHA = 0x0303;
inline constexpr GLenum GL_UNSIGNED_BYTE = 0x1401;
inline constexpr GLenum GL_UNSIGNED_SHORT = 0x1403;
inline constexpr GLenum GL_FLOAT         = 0x1406;
inline constexpr GLenum GL_RGB           = 0x1907;
inline constexpr GLenum GL_FRONT         = 0x0404;
inline constexpr GLenum GL_BACK          = 0x0405;
inline constexpr GLenum GL_RGBA          = 0x1908;
inline constexpr GLenum GL_RGBA8         = 0x8058;
inline constexpr GLenum GL_TEXTURE_2D    = 0x0DE1;
inline constexpr GLenum GL_TEXTURE0      = 0x84C0;
inline constexpr GLenum GL_TEXTURE_MIN_FILTER = 0x2801;
inline constexpr GLenum GL_TEXTURE_MAG_FILTER = 0x2800;
inline constexpr GLenum GL_TEXTURE_WRAP_S = 0x2802;
inline constexpr GLenum GL_TEXTURE_WRAP_T = 0x2803;
inline constexpr GLenum GL_NEAREST       = 0x2600;
inline constexpr GLenum GL_LINEAR        = 0x2601;
inline constexpr GLenum GL_CLAMP_TO_EDGE = 0x812F;
inline constexpr GLenum GL_ARRAY_BUFFER  = 0x8892;
inline constexpr GLenum GL_ELEMENT_ARRAY_BUFFER = 0x8893;
inline constexpr GLenum GL_STATIC_DRAW   = 0x88E4;
inline constexpr GLenum GL_DYNAMIC_DRAW  = 0x88E8;
inline constexpr GLenum GL_FRAGMENT_SHADER = 0x8B30;
inline constexpr GLenum GL_VERTEX_SHADER = 0x8B31;
inline constexpr GLenum GL_COMPILE_STATUS = 0x8B81;
inline constexpr GLenum GL_LINK_STATUS   = 0x8B82;
inline constexpr GLenum GL_INFO_LOG_LENGTH = 0x8B84;
inline constexpr GLenum GL_COLOR_BUFFER_BIT = 0x00004000;
inline constexpr GLenum GL_NO_ERROR      = 0;
inline constexpr GLenum GL_PACK_ALIGNMENT = 0x0D05;
inline constexpr GLenum GL_UNPACK_ALIGNMENT = 0x0CF5;

// --- прототипы --------------------------------------------------------------
using PFN_glEnable = void (*)(GLenum);
using PFN_glDisable = void (*)(GLenum);
using PFN_glViewport = void (*)(GLint, GLint, GLsizei, GLsizei);
using PFN_glScissor = void (*)(GLint, GLint, GLsizei, GLsizei);
using PFN_glClearColor = void (*)(GLfloat, GLfloat, GLfloat, GLfloat);
using PFN_glClear = void (*)(GLbitfield);
using PFN_glBlendFunc = void (*)(GLenum, GLenum);
using PFN_glGetError = GLenum (*)();
using PFN_glGetString = const GLubyte* (*)(GLenum);
using PFN_glGenTextures = void (*)(GLsizei, GLuint*);
using PFN_glDeleteTextures = void (*)(GLsizei, const GLuint*);
using PFN_glBindTexture = void (*)(GLenum, GLuint);
using PFN_glTexImage2D = void (*)(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum,
                                  const GLvoid*);
using PFN_glTexSubImage2D = void (*)(GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLenum,
                                     const GLvoid*);
using PFN_glTexParameteri = void (*)(GLenum, GLenum, GLint);
using PFN_glActiveTexture = void (*)(GLenum);
using PFN_glGenVertexArrays = void (*)(GLsizei, GLuint*);
using PFN_glDeleteVertexArrays = void (*)(GLsizei, const GLuint*);
using PFN_glBindVertexArray = void (*)(GLuint);
using PFN_glGenBuffers = void (*)(GLsizei, GLuint*);
using PFN_glDeleteBuffers = void (*)(GLsizei, const GLuint*);
using PFN_glBindBuffer = void (*)(GLenum, GLuint);
using PFN_glBufferData = void (*)(GLenum, GLsizeiptr, const GLvoid*, GLenum);
using PFN_glBufferSubData = void (*)(GLenum, GLintptr, GLsizeiptr, const GLvoid*);
using PFN_glEnableVertexAttribArray = void (*)(GLuint);
using PFN_glVertexAttribPointer = void (*)(GLuint, GLint, GLenum, GLboolean, GLsizei,
                                           const GLvoid*);
using PFN_glDrawArrays = void (*)(GLenum, GLint, GLsizei);
using PFN_glDrawElements = void (*)(GLenum, GLsizei, GLenum, const GLvoid*);
using PFN_glCreateShader = GLuint (*)(GLenum);
using PFN_glDeleteShader = void (*)(GLuint);
using PFN_glShaderSource = void (*)(GLuint, GLsizei, const GLchar* const*, const GLint*);
using PFN_glCompileShader = void (*)(GLuint);
using PFN_glGetShaderiv = void (*)(GLuint, GLenum, GLint*);
using PFN_glGetShaderInfoLog = void (*)(GLuint, GLsizei, GLsizei*, GLchar*);
using PFN_glCreateProgram = GLuint (*)();
using PFN_glDeleteProgram = void (*)(GLuint);
using PFN_glAttachShader = void (*)(GLuint, GLuint);
using PFN_glDetachShader = void (*)(GLuint, GLuint);
using PFN_glLinkProgram = void (*)(GLuint);
using PFN_glUseProgram = void (*)(GLuint);
using PFN_glGetProgramiv = void (*)(GLuint, GLenum, GLint*);
using PFN_glGetProgramInfoLog = void (*)(GLuint, GLsizei, GLsizei*, GLchar*);
using PFN_glGetUniformLocation = GLint (*)(GLuint, const GLchar*);
using PFN_glUniform1i = void (*)(GLint, GLint);
using PFN_glUniform1f = void (*)(GLint, GLfloat);
using PFN_glUniform2f = void (*)(GLint, GLfloat, GLfloat);
using PFN_glUniform4f = void (*)(GLint, GLfloat, GLfloat, GLfloat, GLfloat);
using PFN_glUniformMatrix4fv = void (*)(GLint, GLsizei, GLboolean, const GLfloat*);
using PFN_glReadPixels = void (*)(GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, GLvoid*);
using PFN_glPixelStorei = void (*)(GLenum, GLint);
using PFN_glReadBuffer = void (*)(GLenum);
using PFN_glDrawBuffer = void (*)(GLenum);

// Набор указателей, заполняемый load_gl_functions(). До загрузки все поля — nullptr.
struct GlFunctions {
    PFN_glEnable glEnable_ = nullptr;
    PFN_glDisable glDisable_ = nullptr;
    PFN_glViewport glViewport_ = nullptr;
    PFN_glScissor glScissor_ = nullptr;
    PFN_glClearColor glClearColor_ = nullptr;
    PFN_glClear glClear_ = nullptr;
    PFN_glBlendFunc glBlendFunc_ = nullptr;
    PFN_glGetError glGetError_ = nullptr;
    PFN_glGetString glGetString_ = nullptr;
    PFN_glGenTextures glGenTextures_ = nullptr;
    PFN_glDeleteTextures glDeleteTextures_ = nullptr;
    PFN_glBindTexture glBindTexture_ = nullptr;
    PFN_glTexImage2D glTexImage2D_ = nullptr;
    PFN_glTexSubImage2D glTexSubImage2D_ = nullptr;
    PFN_glTexParameteri glTexParameteri_ = nullptr;
    PFN_glActiveTexture glActiveTexture_ = nullptr;
    PFN_glGenVertexArrays glGenVertexArrays_ = nullptr;
    PFN_glDeleteVertexArrays glDeleteVertexArrays_ = nullptr;
    PFN_glBindVertexArray glBindVertexArray_ = nullptr;
    PFN_glGenBuffers glGenBuffers_ = nullptr;
    PFN_glDeleteBuffers glDeleteBuffers_ = nullptr;
    PFN_glBindBuffer glBindBuffer_ = nullptr;
    PFN_glBufferData glBufferData_ = nullptr;
    PFN_glBufferSubData glBufferSubData_ = nullptr;
    PFN_glEnableVertexAttribArray glEnableVertexAttribArray_ = nullptr;
    PFN_glVertexAttribPointer glVertexAttribPointer_ = nullptr;
    PFN_glDrawArrays glDrawArrays_ = nullptr;
    PFN_glDrawElements glDrawElements_ = nullptr;
    PFN_glCreateShader glCreateShader_ = nullptr;
    PFN_glDeleteShader glDeleteShader_ = nullptr;
    PFN_glShaderSource glShaderSource_ = nullptr;
    PFN_glCompileShader glCompileShader_ = nullptr;
    PFN_glGetShaderiv glGetShaderiv_ = nullptr;
    PFN_glGetShaderInfoLog glGetShaderInfoLog_ = nullptr;
    PFN_glCreateProgram glCreateProgram_ = nullptr;
    PFN_glDeleteProgram glDeleteProgram_ = nullptr;
    PFN_glAttachShader glAttachShader_ = nullptr;
    PFN_glDetachShader glDetachShader_ = nullptr;
    PFN_glLinkProgram glLinkProgram_ = nullptr;
    PFN_glUseProgram glUseProgram_ = nullptr;
    PFN_glGetProgramiv glGetProgramiv_ = nullptr;
    PFN_glGetProgramInfoLog glGetProgramInfoLog_ = nullptr;
    PFN_glGetUniformLocation glGetUniformLocation_ = nullptr;
    PFN_glUniform1i glUniform1i_ = nullptr;
    PFN_glUniform1f glUniform1f_ = nullptr;
    PFN_glUniform2f glUniform2f_ = nullptr;
    PFN_glUniform4f glUniform4f_ = nullptr;
    PFN_glUniformMatrix4fv glUniformMatrix4fv_ = nullptr;
    PFN_glReadPixels glReadPixels_ = nullptr;
    PFN_glPixelStorei glPixelStorei_ = nullptr;
    PFN_glReadBuffer glReadBuffer_ = nullptr;
    PFN_glDrawBuffer glDrawBuffer_ = nullptr;
    gl::GLenum last_error = 0; // код последней ошибки GL (0 — нет), для отладочной панели
};

// Заполняет набор указателей через loader(name) — обычно SDL_GL_GetProcAddress.
// Возвращает false и пишет в missing (если указан) имя первой ненайденной функции:
// драйвер не поддерживает OpenGL 3.3 Core → движок сообщает об отказе и продолжает
// работу с NullRenderer (п. 4.2.3).
bool load_gl_functions(GlFunctions& out, void* (*loader)(const char*),
                       std::string* missing = nullptr);

// Проверка минимально необходимого набора (без шейдерных функций — smoke-тест)
bool has_gl_base(const GlFunctions& fns);

} // namespace engine::render::gl
