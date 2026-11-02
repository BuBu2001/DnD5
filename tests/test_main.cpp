// tests/test_main.cpp — точка входа тестового прогона (раздел 8 ТЗ DOC-04).
// При переходе на Catch2 (таблица 4.16, docs/decisions.md DR-03) этот файл
// заменяется целью Catch2::Catch2WithMain — исходный код тестов не меняется.
#include "test_harness.h"

int main(int argc, char** argv) {
    // Необязательный аргумент — подстрока-фильтр имён тестов
    const char* filter = (argc > 1) ? argv[1] : nullptr;
    return test_harness::run_all(filter);
}
