// engine/core/result.h — тип результата без исключений.
// Ошибка загрузки ресурса не должна приводить к аварийному завершению (п. 4.1.4.4).
#pragma once

#include <optional>
#include <string>
#include <utility>

namespace engine::core {

template <typename T>
class Result {
public:
    static Result ok(T value) {
        Result r;
        r.value_ = std::move(value);
        return r;
    }
    static Result err(std::string message) {
        Result r;
        r.error_ = std::move(message);
        return r;
    }

    bool is_ok() const { return value_.has_value(); }
    explicit operator bool() const { return is_ok(); }

    T& value() { return *value_; }
    const T& value() const { return *value_; }
    const std::string& error_message() const { return *error_; }

private:
    Result() = default;
    std::optional<T> value_;
    std::optional<std::string> error_;
};

} // namespace engine::core
