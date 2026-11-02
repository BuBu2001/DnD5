#include "core/log.h"

#include <cstdio>
#include <mutex>

namespace engine::core {

static const char* level_name(LogLevel l) {
    switch (l) {
        case LogLevel::Trace: return "TRACE";
        case LogLevel::Debug: return "DEBUG";
        case LogLevel::Info:  return "INFO ";
        case LogLevel::Warn:  return "WARN ";
        case LogLevel::Error: return "ERROR";
    }
    return "?";
}

Log& Log::instance() {
    static Log lg;
    return lg;
}

void Log::set_level(LogLevel min_level) { min_level_ = min_level; }

void Log::log(LogLevel level, const std::string& message) {
    if (level < min_level_) return;
    static std::mutex mtx;
    std::lock_guard<std::mutex> lock(mtx);
    std::fprintf(stderr, "[%s] %s\n", level_name(level), message.c_str());
}

} // namespace engine::core
