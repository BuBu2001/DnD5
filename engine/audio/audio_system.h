// engine/audio/audio_system.h — интерфейс звуковой подсистемы (FR-E50…FR-E54).
// Заглушка до этапа M9: SDL2_mixer, раздельные каналы music/sfx.
#pragma once

#include <cstdint>

namespace engine::audio {

using SoundHandle = std::uint32_t;

class IAudioSystem {
public:
    virtual ~IAudioSystem() = default;
    virtual SoundHandle play_music(const char* virtual_path, bool loop) = 0;
    virtual SoundHandle play_sfx(const char* virtual_path) = 0;
    virtual void set_master_volume(float v) = 0;   // 0..1
    virtual void set_music_volume(float v) = 0;
    virtual void set_sfx_volume(float v) = 0;
    virtual void stop_music() = 0;                  // fade — V1 (п. 4.1.5.2)
};

} // namespace engine::audio
