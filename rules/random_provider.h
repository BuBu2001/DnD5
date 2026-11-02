// rules/random_provider.h — источник случайности через интерфейс (E-D07, п. Г.4).
// DiceRoller не вызывает rand() напрямую: провайдер инжектируется.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace rules {

class IRandomProvider {
public:
    virtual ~IRandomProvider() = default;
    virtual std::uint32_t next_u32() = 0;
    virtual int roll_d20() = 0; // 1..20
};

// Детерминированный генератор xoshiro256** (источник [25]).
// Состояние сериализуется для воспроизводимости боя (FR-E92, NFR-E09, п. 8.4):
// при фиксированном seed последовательность бросков восстанавливается полностью.
class SeededRandomProvider final : public IRandomProvider {
public:
    explicit SeededRandomProvider(std::uint64_t seed);
    std::uint32_t next_u32() override;
    int roll_d20() override;

    // Состояние в виде 64 шестнадцатеричных символов (4 слова по 64 бита).
    // Формат поля "rng_state" файла сохранения (п. 4.1.9.2).
    std::string export_state() const;

    // Восстановление состояния. Возвращает false при недопустимом формате —
    // состояние не изменяется (устойчивость к ошибочным данным, NFR-E14).
    bool restore_state(const std::string& hex_state);

    // Число слов состояния (для проверки формата)
    static constexpr std::size_t kStateWords = 4;

private:
    std::uint64_t state_[4];
    std::uint64_t next();
};

} // namespace rules
