// engine/audio/audio_system.h — звуковая подсистема (FR-E50…FR-E53).
// Форматы OGG и WAV через SDL2_mixer; раздельные каналы музыки и эффектов:
// не менее одного канала музыки и не менее восьми каналов эффектов (FR-E51);
// раздельная громкость master/music/sfx с сохранением в конфигурации (FR-E52).
// Плавное затухание музыки и приоритет эффектов — статус V1 (п. 4.1.5.2).
#pragma once

#include <cstdint>
#include <string>

namespace engine::audio {

using SoundHandle = std::uint32_t;
inline constexpr SoundHandle kInvalidSound = 0;

// Требуемое число каналов эффектов (FR-E51)
inline constexpr int kMinSfxChannels = 8;
inline constexpr int kMusicChannels = 1;

struct AudioSettings {
    float master = 0.8f;   // 0..1 (FR-E52)
    float music = 0.6f;
    float sfx = 0.9f;
    bool muted = false;    // полное отключение звука (п. 4.1.5.2, V1)
};

class IAudioSystem {
public:
    virtual ~IAudioSystem() = default;

    // Инициализация подсистемы: false при ошибке — программа продолжает работу
    // без звука (п. 4.2.3: отказ без аварийного завершения)
    virtual bool init(int frequency = 44100, int sfx_channels = kMinSfxChannels) = 0;
    virtual void shutdown() = 0;

    // Воспроизведение: пути — виртуальные assets:/… (DATA-E05)
    virtual SoundHandle play_music(const std::string& virtual_path, bool loop) = 0;
    virtual SoundHandle play_sfx(const std::string& virtual_path) = 0;
    virtual void stop_music() = 0;
    virtual void stop_all_sfx() = 0;

    virtual void set_settings(const AudioSettings& settings) = 0;
    virtual const AudioSettings& settings() const = 0;

    virtual void set_master_volume(float v) = 0;   // 0..1
    virtual void set_music_volume(float v) = 0;
    virtual void set_sfx_volume(float v) = 0;
    virtual void set_muted(bool muted) = 0;

    // Метрики для отладочной панели (FR-E100)
    virtual int active_sfx_channels() const = 0;
    virtual std::size_t loaded_sounds() const = 0;
};

// SDL2_mixer-реализация (этап M9). Объявлена всегда, определена при DND_WITH_SDL2.
class SdlAudioSystem final : public IAudioSystem {
public:
    SdlAudioSystem();
    ~SdlAudioSystem() override;

    bool init(int frequency = 44100, int sfx_channels = kMinSfxChannels) override;
    void shutdown() override;

    SoundHandle play_music(const std::string& virtual_path, bool loop) override;
    SoundHandle play_sfx(const std::string& virtual_path) override;
    void stop_music() override;
    void stop_all_sfx() override;

    void set_settings(const AudioSettings& settings) override;
    const AudioSettings& settings() const override { return settings_; }

    void set_master_volume(float v) override;
    void set_music_volume(float v) override;
    void set_sfx_volume(float v) override;
    void set_muted(bool muted) override;

    int active_sfx_channels() const override;
    std::size_t loaded_sounds() const override;

private:
    void apply_volumes();

    struct Impl;
    Impl* impl_ = nullptr;
    AudioSettings settings_{};
    bool initialized_ = false;
};

// Заглушка «без звука»: используется в headless-сборке и при отказе аудиоустройства,
// чтобы игровой код не проверял наличие звука в каждом вызове (п. 4.2.1)
class NullAudioSystem final : public IAudioSystem {
public:
    bool init(int = 44100, int = kMinSfxChannels) override { return true; }
    void shutdown() override {}
    SoundHandle play_music(const std::string&, bool) override { return kInvalidSound; }
    SoundHandle play_sfx(const std::string&) override { return kInvalidSound; }
    void stop_music() override {}
    void stop_all_sfx() override {}
    void set_settings(const AudioSettings& s) override { settings_ = s; }
    const AudioSettings& settings() const override { return settings_; }
    void set_master_volume(float v) override { settings_.master = v; }
    void set_music_volume(float v) override { settings_.music = v; }
    void set_sfx_volume(float v) override { settings_.sfx = v; }
    void set_muted(bool m) override { settings_.muted = m; }
    int active_sfx_channels() const override { return 0; }
    std::size_t loaded_sounds() const override { return 0; }

private:
    AudioSettings settings_{};
};

} // namespace engine::audio
