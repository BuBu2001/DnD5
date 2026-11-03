// tests/test_main.cpp — точка входа тестового прогона (раздел 8 ТЗ DOC-04).
// При переходе на Catch2 (таблица 4.16, docs/decisions.md DR-03) этот файл
// заменяется целью Catch2::Catch2WithMain — исходный код тестов не меняется.
// В режиме Catch2 точка входа main() генерируется библиотекой в ЭТОЙ
// трансляционной единице (CATCH_CONFIG_MAIN определяется до включения).
#if defined(DND_WITH_CATCH2) && defined(__has_include)
#  if __has_include(<catch2/catch.hpp>)
#    define CATCH_CONFIG_MAIN
#  endif
#endif

#include "test_harness.h"

// В режиме Catch2 собственный main не определяется.
#ifndef DND_TEST_HARNESS_CATCH2
int main(int argc, char** argv) {
    // Необязательный аргумент — подстрока-фильтр имён тестов
    const char* filter = (argc > 1) ? argv[1] : nullptr;
    return test_harness::run_all(filter);
}
#endif
