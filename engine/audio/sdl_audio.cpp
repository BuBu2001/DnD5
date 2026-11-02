// engine/audio/sdl_audio.cpp — звуковая подсистема на SDL2_mixer (FR-E50…FR-E52).
#include "audio/audio_system.h"

#ifdef DND_ENGINE_WITH_SDL2

#include <SDL.h>
#include <SDL_mixer.h>

#include <unordered_map>

#include "core/file_system.h"
#include "core/log.h"

namespace engine::audio {

namespace {

constexpr int kMixerMaxVolume = 128; // MIX_MAX_VOLUME

float clamp01(float v) { return v < 0.f ? 0.f : (v > 1.f ? 1.f : v); }

int to_mixer_volume(float normalized) {
    return static_cast<int>(clamp01(normalized) * static_cast<float>(kMixerMaxVolume));
}

} // namespace

struct SdlAudioSystem::Impl {
    std::unordered_map<std::string, Mix_Chunk*> chunks; // кэш эффектов (FR-E45-подобный)
    Mix_Music* music = nullptr;
    std::string music_path;
    int sfx_channels = kMinSfxChannels;
    SoundHandle next_handle = 1;
    core::FileSystem fs{"."}; // физический путь вычисляется из виртуального
};

SdlAudioSystem::SdlAudioSystem() : impl_(new Impl()) {}

SdlAudioSystem::~SdlAudioSystem() {
    shutdown();
    delete impl_;
    impl_ = nullptr;
}

bool SdlAudioSystem::init(int frequency, int sfx_channels) {
    if (initialized_) return true;

    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
        core::Log::instance().error(std::string("audio: SDL_InitSubSystem failed: ") +
                                    SDL_GetError());
        return false; // работа без звука допустима (п. 4.2.3)
    }

    if (Mix_OpenAudio(frequency, MIX_DEFAULT_FORMAT, 2, 2048) != 0) {
        core::Log::instance().error(std::string("audio: Mix_OpenAudio failed: ") + Mix_GetError());
        return false;
    }

    impl_->sfx_channels = sfx_channels < kMinSfxChannels ? kMinSfxChannels : sfx_channels;
    Mix_AllocateChannels(impl_->sfx_channels); // FR-E51: не менее 8 каналов эффектов
    initialized_ = true;
    apply_volumes();
    core::Log::instance().info("audio: SDL2_mixer инициализирован, каналов эффектов: " +
                               std::to_string(impl_->sfx_channels));
    return true;
}

void SdlAudioSystem::shutdown() {
    if (!initialized_) return;
    if (impl_->music != nullptr) {
        Mix_FreeMusic(impl_->music);
        impl_->music = nullptr;
    }
    for (auto& [path, chunk] : impl_->chunks) {
        if (chunk != nullptr) Mix_FreeChunk(chunk);
    }
    impl_->chunks.clear();
    Mix_CloseAudio();
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
    initialized_ = false;
}

void SdlAudioSystem::apply_volumes() {
    if (!initialized_) return;
    const float master = settings_.muted ? 0.f : clamp01(settings_.master);
    Mix_VolumeMusic(to_mixer_volume(master * clamp01(settings_.music)));
    // Громкость эффектов задаётся на канал (FR-E52: раздельно master/music/sfx)
    const int channel_volume = to_mixer_volume(master * clamp01(settings_.sfx));
    for (int i = 0; i < impl_->sfx_channels; ++i) Mix_Volume(i, channel_volume);
}

SoundHandle SdlAudioSystem::play_music(const std::string& virtual_path, bool loop) {
    if (!initialized_) return kInvalidSound;

    // Тот же трек — не перезагружаем (экономия памяти, NFR-E05)
    if (impl_->music_path == virtual_path && impl_->music != nullptr && Mix_PlayingMusic() != 0) {
        return impl_->next_handle;
    }

    if (impl_->music != nullptr) {
        Mix_FreeMusic(impl_->music);
        impl_->music = nullptr;
    }

    const auto physical = impl_->fs.resolve(virtual_path);
    if (!physical) {
        core::Log::instance().error("audio: " + physical.error_message());
        return kInvalidSound;
    }
    impl_->music = Mix_LoadMUS(physical.value().c_str());
    if (impl_->music == nullptr) {
        // Ошибка загрузки ресурса: журнал + продолжение работы (п. 4.1.4.4)
        core::Log::instance().error("audio: не удалось загрузить музыку " + virtual_path + ": " +
                                    Mix_GetError());
        impl_->music_path.clear();
        return kInvalidSound;
    }
    impl_->music_path = virtual_path;
    if (Mix_PlayMusic(impl_->music, loop ? -1 : 0) != 0) {
        core::Log::instance().error(std::string("audio: Mix_PlayMusic failed: ") + Mix_GetError());
        return kInvalidSound;
    }
    return impl_->next_handle++;
}

SoundHandle SdlAudioSystem::play_sfx(const std::string& virtual_path) {
    if (!initialized_ || settings_.muted) return kInvalidSound;

    auto it = impl_->chunks.find(virtual_path);
    if (it == impl_->chunks.end()) {
        const auto physical = impl_->fs.resolve(virtual_path);
        if (!physical) {
            core::Log::instance().error("audio: " + physical.error_message());
            return kInvalidSound;
        }
        Mix_Chunk* chunk = Mix_LoadWAV(physical.value().c_str());
        if (chunk == nullptr) {
            core::Log::instance().error("audio: не удалось загрузить эффект " + virtual_path +
                                        ": " + Mix_GetError());
            return kInvalidSound;
        }
        it = impl_->chunks.emplace(virtual_path, chunk).first;
    }
    // Канал выбирается автоматически; при нехватке SDL2_mixer вытесняет старейший
    if (Mix_PlayChannel(-1, it->second, 0) == -1) {
        core::Log::instance().warn("audio: нет свободного канала для " + virtual_path);
        return kInvalidSound;
    }
    return impl_->next_handle++;
}

void SdlAudioSystem::stop_music() {
    if (initialized_) Mix_HaltMusic();
}

void SdlAudioSystem::stop_all_sfx() {
    if (initialized_) Mix_HaltChannel(-1);
}

void SdlAudioSystem::set_settings(const AudioSettings& settings) {
    settings_ = settings;
    apply_volumes();
}

void SdlAudioSystem::set_master_volume(float v) {
    settings_.master = clamp01(v);
    apply_volumes();
}

void SdlAudioSystem::set_music_volume(float v) {
    settings_.music = clamp01(v);
    apply_volumes();
}

void SdlAudioSystem::set_sfx_volume(float v) {
    settings_.sfx = clamp01(v);
    apply_volumes();
}

void SdlAudioSystem::set_muted(bool muted) {
    settings_.muted = muted;
    apply_volumes();
}

int SdlAudioSystem::active_sfx_channels() const {
    if (!initialized_) return 0;
    int active = 0;
    for (int i = 0; i < impl_->sfx_channels; ++i) {
        if (Mix_Playing(i) != 0) ++active;
    }
    return active;
}

std::size_t SdlAudioSystem::loaded_sounds() const {
    return impl_ != nullptr ? impl_->chunks.size() : 0;
}

} // namespace engine::audio

#endif // DND_ENGINE_WITH_SDL2
