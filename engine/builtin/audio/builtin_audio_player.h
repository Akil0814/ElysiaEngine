#pragma once

#include "../../audio/audio_settings.h"

#include <string_view>

namespace elysia::builtin
{
class BuiltinAssetCache;

class BuiltinAudioPlayer
{
public:
    void bind(const BuiltinAssetCache& cache,const elysia::audio::AudioSettings& settings) noexcept;
    void unbind() noexcept;

    [[nodiscard]] bool bound() const noexcept;
    [[nodiscard]] int play_sound(std::string_view key, int loops = 0) const;
    [[nodiscard]] bool play_music(std::string_view key, int loops = -1) const;
    void stop_music() const noexcept;

    void set_master_volume(int volume) noexcept;
    void set_music_volume(int volume) noexcept;
    void set_sound_volume(int volume) noexcept;
    [[nodiscard]] const elysia::audio::AudioSettings& settings() const noexcept;

private:
    [[nodiscard]] static int clamp_volume(int volume) noexcept;
    [[nodiscard]] static int to_mix_volume(int volume) noexcept;

private:
    const BuiltinAssetCache* _cache = nullptr;
    elysia::audio::AudioSettings _settings{};
};
}
