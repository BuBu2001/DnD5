// tests/test_harness.h — минимальный каркас модульного тестирования.
//
// ПРОЕКТНОЕ РЕШЕНИЕ (docs/decisions.md, DR-10): таблица 4.16 ТЗ DOC-04 фиксирует
// Catch2 (BSL-1.0) как фреймворк тестов. Библиотека ещё не внесена в third_party/,
// поэтому используется совместимый по синтаксису микро-каркас с тем же набором
// макросов: TEST_CASE, SECTION, REQUIRE, REQUIRE_FALSE, CHECK, INFO.
// Переход на Catch2 выполняется заменой этого заголовка на <catch2/catch_all.hpp>
// и подключением цели Catch2::Catch2WithMain — исходный код тестов не меняется.
#pragma once

#include <cmath>
#include <cstdio>
#include <exception>
#include <functional>
#include <string>
#include <vector>

namespace test_harness {

struct Stats {
    std::size_t cases = 0;
    std::size_t checks = 0;
    std::size_t failures = 0;
};

inline Stats& stats() {
    static Stats s;
    return s;
}

struct TestCase {
    std::string name;
    std::function<void()> body;
};

inline std::vector<TestCase>& registry() {
    static std::vector<TestCase> r;
    return r;
}

struct Registrar {
    Registrar(std::string name, std::function<void()> body) {
        registry().push_back({std::move(name), std::move(body)});
    }
};

// Исключение проверки: прерывает текущий TEST_CASE, остальные продолжают работу
// (устойчивость прогона — аналог поведения Catch2)
struct CheckFailure {
    std::string message;
};

inline int run_all(const char* filter = nullptr) {
    std::size_t executed = 0;
    for (const auto& tc : registry()) {
        if (filter != nullptr && tc.name.find(filter) == std::string::npos) continue;
        ++executed;
        ++stats().cases;
        std::printf("[ RUN  ] %s\n", tc.name.c_str());
        try {
            tc.body();
        } catch (const CheckFailure& f) {
            std::printf("         прервано: %s\n", f.message.c_str());
        } catch (const std::exception& e) {
            ++stats().failures;
            std::printf("[ EXC  ] %s: %s\n", tc.name.c_str(), e.what());
        } catch (...) {
            ++stats().failures;
            std::printf("[ EXC  ] %s: неизвестное исключение\n", tc.name.c_str());
        }
    }
    std::printf("\n%zu test cases, %zu checks, %zu failures\n", stats().cases, stats().checks,
                stats().failures);
    if (executed == 0) std::printf("ПРЕДУПРЕЖДЕНИЕ: ни один тест не выполнен (фильтр?)\n");
    return stats().failures == 0 ? 0 : 1;
}

} // namespace test_harness

#define TH_CAT2(a, b) a##b
#define TH_CAT(a, b) TH_CAT2(a, b)

// Имя кейса — первый аргумент; теги Catch2 (второй и далее) игнорируются,
// чтобы исходный код тестов оставался совместимым с Catch2 (DR-03).
#define TH_FIRST(first, ...) first

#define TEST_CASE(...)                                                        \
    static void TH_CAT(th_case_, __LINE__)();                                 \
    [[maybe_unused]] static ::test_harness::Registrar TH_CAT(th_reg_, __LINE__)(  \
        TH_FIRST(__VA_ARGS__, ignored), &TH_CAT(th_case_, __LINE__));         \
    static void TH_CAT(th_case_, __LINE__)()

#define SECTION(...) if (bool th_section = true; th_section)

#define REQUIRE(expr)                                                         \
    do {                                                                      \
        ++::test_harness::stats().checks;                                     \
        if (!(expr)) {                                                        \
            ++::test_harness::stats().failures;                               \
            std::printf("  FAIL %s:%d: REQUIRE(%s)\n", __FILE__, __LINE__, #expr); \
            throw ::test_harness::CheckFailure{#expr};                        \
        }                                                                     \
    } while (0)

#define REQUIRE_FALSE(expr) REQUIRE(!(expr))

#define CHECK(expr)                                                           \
    do {                                                                      \
        ++::test_harness::stats().checks;                                     \
        if (!(expr)) {                                                        \
            ++::test_harness::stats().failures;                               \
            std::printf("  FAIL %s:%d: CHECK(%s)\n", __FILE__, __LINE__, #expr); \
        }                                                                     \
    } while (0)

#define REQUIRE_THAT(value, matcher) REQUIRE(matcher(value))

#define INFO(msg) std::printf("  INFO: %s\n", static_cast<const char*>(msg))

// Точное сравнение чисел с плавающей точкой (для вероятностей, п. Г.13)
inline bool approx_equal(double a, double b, double eps = 1e-9) {
    return std::fabs(a - b) <= eps;
}
