// engine/core/log.h — логирование (FR-E08). Требование: журнал с уровнями,
// номер версии сообщают в журнале при запуске (п. 4.6.1).
#pragma once

#include <sstream>
#include <string>

namespace engine::core {

enum class LogLevel { Trace, Debug, Info, Warn, Error };

class Log {
public:
    static Log& instance();

    void set_level(LogLevel min_level);
    void log(LogLevel level, const std::string& message);

    void trace(const std::string& m) { log(LogLevel::Trace, m); }
    void debug(const std::string& m) { log(LogLevel::Debug, m); }
    void info(const std::string& m)  { log(LogLevel::Info, m); }
    void warn(const std::string& m)  { log(LogLevel::Warn, m); }
    void error(const std::string& m) { log(LogLevel::Error, m); }

private:
    Log() = default;
    LogLevel min_level_ = LogLevel::Debug;
};

template <typename... Args>
std::string strcat(Args&&... args) {
    std::ostringstream oss;
    (oss << ... << std::forward<Args>(args));
    return oss.str();
}

} // namespace engine::core
