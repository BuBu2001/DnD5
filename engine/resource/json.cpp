// engine/resource/json.cpp — реализация разбора и сериализации JSON (FR-E41, FR-E48).
#include "resource/json.h"

#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <sstream>

namespace engine::resource {

namespace {

const JsonArray kEmptyArray{};
const JsonObject kEmptyObject{};
const std::string kEmptyString{};
const JsonValue kNullValue{};

// Экранирование строки по RFC 8259; не-ASCII байты UTF-8 проходят как есть (п. 4.5.5)
void append_escaped(std::string& out, const std::string& s) {
    out.push_back('"');
    for (const char ch : s) {
        const auto c = static_cast<unsigned char>(ch);
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else {
                    out.push_back(ch);
                }
        }
    }
    out.push_back('"');
}

void append_indent(std::string& out, int indent, int depth) {
    if (indent <= 0) return;
    out.push_back('\n');
    out.append(static_cast<std::size_t>(indent * depth), ' ');
}

// Разбор UTF-8 последовательности \uXXXX с поддержкой суррогатных пар
bool append_codepoint(std::string& out, unsigned long cp) {
    if (cp <= 0x7F) {
        out.push_back(static_cast<char>(cp));
    } else if (cp <= 0x7FF) {
        out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp <= 0xFFFF) {
        out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp <= 0x10FFFF) {
        out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else {
        return false;
    }
    return true;
}

class Parser {
public:
    explicit Parser(const std::string& text) : text_(text) {}

    core::Result<JsonValue> run() {
        skip_ws();
        auto value = parse_value("");
        if (!value.is_ok()) return value;
        skip_ws();
        if (pos_ != text_.size()) {
            return core::Result<JsonValue>::err(fail("лишние данные после значения JSON"));
        }
        return value;
    }

private:
    std::string fail(const std::string& message) const {
        JsonParseError e;
        e.path = current_path_.empty() ? "<корень>" : current_path_;
        e.line = line_;
        e.column = column_;
        e.message = message;
        return e.to_string();
    }

    void advance() {
        if (pos_ >= text_.size()) return;
        if (text_[pos_] == '\n') { ++line_; column_ = 1; } else { ++column_; }
        ++pos_;
    }

    char peek() const { return pos_ < text_.size() ? text_[pos_] : '\0'; }
    bool eof() const { return pos_ >= text_.size(); }

    void skip_ws() {
        while (!eof()) {
            const char c = peek();
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') advance();
            else break;
        }
    }

    bool consume(char c) {
        skip_ws();
        if (peek() != c) return false;
        advance();
        return true;
    }

    core::Result<JsonValue> parse_value(const std::string& path) {
        const std::string saved = current_path_;
        current_path_ = path;
        skip_ws();
        if (eof()) return core::Result<JsonValue>::err(fail("неожиданный конец файла"));

        const char c = peek();
        core::Result<JsonValue> out = core::Result<JsonValue>::err(fail("некорректное значение"));
        if (c == '{') out = parse_object(path);
        else if (c == '[') out = parse_array(path);
        else if (c == '"') out = parse_string_value();
        else if (c == 't' || c == 'f') out = parse_bool();
        else if (c == 'n') out = parse_null();
        else if (c == '-' || (c >= '0' && c <= '9')) out = parse_number();
        else out = core::Result<JsonValue>::err(fail(std::string("неожиданный символ '") + c + "'"));

        current_path_ = saved;
        return out;
    }

    core::Result<JsonValue> parse_object(const std::string& path) {
        advance(); // '{'
        JsonObject obj;
        skip_ws();
        if (peek() == '}') { advance(); return core::Result<JsonValue>::ok(JsonValue(std::move(obj))); }
        while (true) {
            skip_ws();
            if (peek() != '"') {
                return core::Result<JsonValue>::err(fail("ожидается строковый ключ объекта"));
            }
            auto key = parse_string_raw();
            if (!key.is_ok()) return core::Result<JsonValue>::err(key.error_message());
            if (!consume(':')) {
                return core::Result<JsonValue>::err(fail("ожидается ':' после ключа '" + key.value() + "'"));
            }
            const std::string child_path = path.empty() ? key.value() : path + "." + key.value();
            auto value = parse_value(child_path);
            if (!value.is_ok()) return value;
            obj.insert_or_assign(key.value(), std::move(value.value()));
            skip_ws();
            if (consume(',')) continue;
            if (consume('}')) break;
            return core::Result<JsonValue>::err(fail("ожидается ',' или '}' в объекте"));
        }
        return core::Result<JsonValue>::ok(JsonValue(std::move(obj)));
    }

    core::Result<JsonValue> parse_array(const std::string& path) {
        advance(); // '['
        JsonArray arr;
        skip_ws();
        if (peek() == ']') { advance(); return core::Result<JsonValue>::ok(JsonValue(std::move(arr))); }
        std::size_t index = 0;
        while (true) {
            const std::string child_path = path + "[" + std::to_string(index) + "]";
            auto value = parse_value(child_path);
            if (!value.is_ok()) return value;
            arr.push_back(std::move(value.value()));
            ++index;
            skip_ws();
            if (consume(',')) continue;
            if (consume(']')) break;
            return core::Result<JsonValue>::err(fail("ожидается ',' или ']' в массиве"));
        }
        return core::Result<JsonValue>::ok(JsonValue(std::move(arr)));
    }

    core::Result<std::string> parse_string_raw() {
        advance(); // '"'
        std::string out;
        while (true) {
            if (eof()) return core::Result<std::string>::err(fail("незакрытая строка"));
            const char c = peek();
            if (c == '"') { advance(); break; }
            if (c == '\\') {
                advance();
                const char e = peek();
                advance();
                switch (e) {
                    case '"':  out.push_back('"'); break;
                    case '\\': out.push_back('\\'); break;
                    case '/':  out.push_back('/'); break;
                    case 'b':  out.push_back('\b'); break;
                    case 'f':  out.push_back('\f'); break;
                    case 'n':  out.push_back('\n'); break;
                    case 'r':  out.push_back('\r'); break;
                    case 't':  out.push_back('\t'); break;
                    case 'u': {
                        auto cp = read_hex4();
                        if (cp < 0) return core::Result<std::string>::err(fail("некорректная \\u-последовательность"));
                        auto code = static_cast<unsigned long>(cp);
                        if (code >= 0xD800 && code <= 0xDBFF) {
                            if (peek() == '\\' && text_[pos_ + 1] == 'u') {
                                advance(); advance();
                                const auto low = read_hex4();
                                if (low >= 0xDC00 && low <= 0xDFFF) {
                                    code = 0x10000 + ((code - 0xD800) << 10) +
                                           (static_cast<unsigned long>(low) - 0xDC00);
                                } else {
                                    return core::Result<std::string>::err(fail("некорректная суррогатная пара"));
                                }
                            } else {
                                return core::Result<std::string>::err(fail("ожидается низкий суррогат"));
                            }
                        }
                        if (!append_codepoint(out, code)) {
                            return core::Result<std::string>::err(fail("код символа вне диапазона Unicode"));
                        }
                        break;
                    }
                    default:
                        return core::Result<std::string>::err(fail(std::string("некорректный escape '\\") + e + "'"));
                }
                continue;
            }
            if (static_cast<unsigned char>(c) < 0x20) {
                return core::Result<std::string>::err(fail("управляющий символ в строке недопустим"));
            }
            out.push_back(c);
            advance();
        }
        return core::Result<std::string>::ok(out);
    }

    int read_hex4() {
        int value = 0;
        for (int i = 0; i < 4; ++i) {
            if (eof()) return -1;
            const char c = peek();
            advance();
            int digit = 0;
            if (c >= '0' && c <= '9') digit = c - '0';
            else if (c >= 'a' && c <= 'f') digit = c - 'a' + 10;
            else if (c >= 'A' && c <= 'F') digit = c - 'A' + 10;
            else return -1;
            value = value * 16 + digit;
        }
        return value;
    }

    core::Result<JsonValue> parse_string_value() {
        auto s = parse_string_raw();
        if (!s.is_ok()) return core::Result<JsonValue>::err(s.error_message());
        return core::Result<JsonValue>::ok(JsonValue(std::move(s.value())));
    }

    core::Result<JsonValue> parse_bool() {
        if (text_.compare(pos_, 4, "true") == 0) {
            for (int i = 0; i < 4; ++i) advance();
            return core::Result<JsonValue>::ok(JsonValue(true));
        }
        if (text_.compare(pos_, 5, "false") == 0) {
            for (int i = 0; i < 5; ++i) advance();
            return core::Result<JsonValue>::ok(JsonValue(false));
        }
        return core::Result<JsonValue>::err(fail("ожидается true или false"));
    }

    core::Result<JsonValue> parse_null() {
        if (text_.compare(pos_, 4, "null") == 0) {
            for (int i = 0; i < 4; ++i) advance();
            return core::Result<JsonValue>::ok(JsonValue());
        }
        return core::Result<JsonValue>::err(fail("ожидается null"));
    }

    core::Result<JsonValue> parse_number() {
        const std::size_t start = pos_;
        bool is_double = false;
        if (peek() == '-') advance();
        while (!eof() && peek() >= '0' && peek() <= '9') advance();
        if (peek() == '.') {
            is_double = true;
            advance();
            while (!eof() && peek() >= '0' && peek() <= '9') advance();
        }
        if (peek() == 'e' || peek() == 'E') {
            is_double = true;
            advance();
            if (peek() == '+' || peek() == '-') advance();
            while (!eof() && peek() >= '0' && peek() <= '9') advance();
        }
        const std::string token = text_.substr(start, pos_ - start);
        if (token.empty() || token == "-") {
            return core::Result<JsonValue>::err(fail("некорректное число"));
        }
        if (is_double) {
            return core::Result<JsonValue>::ok(JsonValue(std::strtod(token.c_str(), nullptr)));
        }
        // целое: проверяем переполнение int64 и переходим в double при необходимости
        errno = 0;
        const long long iv = std::strtoll(token.c_str(), nullptr, 10);
        if (errno != 0) {
            return core::Result<JsonValue>::ok(JsonValue(std::strtod(token.c_str(), nullptr)));
        }
        return core::Result<JsonValue>::ok(JsonValue(static_cast<std::int64_t>(iv)));
    }

    const std::string& text_;
    std::size_t pos_ = 0;
    std::int32_t line_ = 1;
    std::int32_t column_ = 1;
    std::string current_path_;
};

} // namespace

std::string JsonParseError::to_string() const {
    std::ostringstream os;
    os << path << ": " << message << " (строка " << line << ", колонка " << column << ")";
    return os.str();
}

core::Result<JsonValue> JsonParser::parse(const std::string& text) {
    Parser parser(text);
    return parser.run();
}

// --- JsonValue ---------------------------------------------------------------

bool JsonValue::as_bool(bool default_value) const {
    if (is_bool()) return std::get<bool>(data_);
    return default_value;
}

std::int64_t JsonValue::as_int(std::int64_t default_value) const {
    if (type() == JsonType::Int) return std::get<std::int64_t>(data_);
    if (type() == JsonType::Double) return static_cast<std::int64_t>(std::get<double>(data_));
    return default_value;
}

double JsonValue::as_double(double default_value) const {
    if (type() == JsonType::Double) return std::get<double>(data_);
    if (type() == JsonType::Int) return static_cast<double>(std::get<std::int64_t>(data_));
    return default_value;
}

const std::string& JsonValue::as_string() const {
    return is_string() ? std::get<std::string>(data_) : kEmptyString;
}

std::string JsonValue::as_string_or(const std::string& d) const {
    return is_string() ? std::get<std::string>(data_) : d;
}

const JsonArray& JsonValue::as_array() const {
    return is_array() ? std::get<JsonArray>(data_) : kEmptyArray;
}

const JsonObject& JsonValue::as_object() const {
    return is_object() ? std::get<JsonObject>(data_) : kEmptyObject;
}

bool JsonValue::contains(const std::string& key) const { return find(key) != nullptr; }

const JsonValue* JsonValue::find(const std::string& key) const {
    if (!is_object()) return nullptr;
    const auto& obj = std::get<JsonObject>(data_);
    const auto it = obj.find(key);
    return it == obj.end() ? nullptr : &it->second;
}

const JsonValue& JsonValue::at(const std::string& key) const {
    const JsonValue* v = find(key);
    return v != nullptr ? *v : kNullValue;
}

std::size_t JsonValue::size() const {
    if (is_array()) return std::get<JsonArray>(data_).size();
    if (is_object()) return std::get<JsonObject>(data_).size();
    return 0;
}

const JsonValue* JsonValue::find_path(const std::string& path) const {
    const JsonValue* current = this;
    std::size_t start = 0;
    while (start <= path.size() && current != nullptr) {
        std::size_t end = path.find('/', start);
        if (end == std::string::npos) end = path.size();
        const std::string token = path.substr(start, end - start);
        if (!token.empty()) {
            if (current->is_object()) {
                current = current->find(token);
            } else if (current->is_array()) {
                const auto& arr = current->as_array();
                char* stop = nullptr;
                const long idx = std::strtol(token.c_str(), &stop, 10);
                if (stop == token.c_str() || idx < 0 ||
                    static_cast<std::size_t>(idx) >= arr.size()) {
                    return nullptr;
                }
                current = &arr[static_cast<std::size_t>(idx)];
            } else {
                return nullptr;
            }
        }
        start = end + 1;
        if (end == path.size()) break;
    }
    return current;
}

JsonValue& JsonValue::operator[](const std::string& key) {
    if (!is_object()) data_ = JsonObject{};
    return std::get<JsonObject>(data_)[key];
}

void JsonValue::push_back(JsonValue v) {
    if (!is_array()) data_ = JsonArray{};
    std::get<JsonArray>(data_).push_back(std::move(v));
}

std::string JsonValue::dump(int indent) const {
    std::string out;
    dump_impl(out, indent, 0);
    return out;
}

void JsonValue::dump_impl(std::string& out, int indent, int depth) const {
    switch (type()) {
        case JsonType::Null:   out += "null"; break;
        case JsonType::Bool:   out += std::get<bool>(data_) ? "true" : "false"; break;
        case JsonType::Int:    out += std::to_string(std::get<std::int64_t>(data_)); break;
        case JsonType::Double: {
            const double d = std::get<double>(data_);
            char buf[32];
            std::snprintf(buf, sizeof(buf), "%.17g", d);
            out += buf;
            break;
        }
        case JsonType::String: append_escaped(out, std::get<std::string>(data_)); break;
        case JsonType::Array: {
            const auto& arr = std::get<JsonArray>(data_);
            if (arr.empty()) { out += "[]"; break; }
            out.push_back('[');
            for (std::size_t i = 0; i < arr.size(); ++i) {
                if (i != 0) out.push_back(',');
                append_indent(out, indent, depth + 1);
                arr[i].dump_impl(out, indent, depth + 1);
            }
            append_indent(out, indent, depth);
            out.push_back(']');
            break;
        }
        case JsonType::Object: {
            const auto& obj = std::get<JsonObject>(data_);
            if (obj.empty()) { out += "{}"; break; }
            out.push_back('{');
            bool first = true;
            for (const auto& [key, value] : obj) {
                if (!first) out.push_back(',');
                first = false;
                append_indent(out, indent, depth + 1);
                append_escaped(out, key);
                out.push_back(':');
                if (indent > 0) out.push_back(' ');
                value.dump_impl(out, indent, depth + 1);
            }
            append_indent(out, indent, depth);
            out.push_back('}');
            break;
        }
    }
}

} // namespace engine::resource
