#include "builtin_audio_player.h"

#include "../resources/builtin_asset_cache.h"
#include "../../tools/logger.h"

#include <SDL_mixer.h>

#include <algorithm>

namespace elysia::builtin
{
void BuiltinAudioPlayer::bind(const BuiltinAssetCache& cache,const elysia::audio::AudioSettings& settings) noexcept
{
    _cache = &cache;
    _settings.master_volume = clamp_volume(settings.master_volume);
    _settings.music_volume = clamp_volume(settings.music_volume);
    _settings.sound_volume = clamp_volume(settings.sound_volume);
}

void BuiltinAudioPlayer::unbind() noexcept
{
    _cache = nullptr;
}

bool BuiltinAudioPlayer::bound() const noexcept
{
    return _cache != nullptr;
}

int BuiltinAudioPlayer::play_sound(std::string_view key, int loops) const
{
    if (!_cache)
    {
        ELYSIA_LOG_WARN("builtin","Play sound failed: audio player is not bound.");
        return -1;
    }

    Mix_Chunk* sound = _cache->find_sound(key);
    if (!sound)
    {
        ELYSIA_LOG_WARN("builtin","Play sound failed: built-in sound does not exist: " << key);
        return -1;
    }

    const int channel = Mix_PlayChannel(-1,sound,loops);
    if (channel < 0)
    {
        ELYSIA_LOG_WARN("builtin","Play sound failed: " << key << " error: " << Mix_GetError());
        return -1;
    }

    const int effective_volume =(_settings.master_volume * _settings.sound_volume) / 100;
    Mix_Volume(channel,to_mix_volume(effective_volume));

    return channel;
}

bool BuiltinAudioPlayer::play_music(std::string_view key, int loops) const
{
    if (!_cache)
    {
        ELYSIA_LOG_WARN("builtin","Play music failed: audio player is not bound.");
        return false;
    }

    Mix_Music* music = _cache->find_music(key);
    if (!music)
    {
        ELYSIA_LOG_WARN("builtin","Play music failed: built-in music does not exist: " << key);
        return false;
    }

    const int effective_volume =
        (_settings.master_volume * _settings.music_volume) / 100;
    Mix_VolumeMusic(to_mix_volume(effective_volume));
    if (Mix_PlayMusic(music,loops) != 0)
    {
        ELYSIA_LOG_WARN("builtin","Play music failed: " << key << " error: " << Mix_GetError());
        return false;
    }

    return true;
}

void BuiltinAudioPlayer::stop_music() const noexcept
{
    Mix_HaltMusic();
}

void BuiltinAudioPlayer::set_master_volume(int volume) noexcept
{
    _settings.master_volume = clamp_volume(volume);
}

void BuiltinAudioPlayer::set_music_volume(int volume) noexcept
{
    _settings.music_volume = clamp_volume(volume);
}

void BuiltinAudioPlayer::set_sound_volume(int volume) noexcept
{
    _settings.sound_volume = clamp_volume(volume);
}

const elysia::audio::AudioSettings& BuiltinAudioPlayer::settings() const noexcept
{
    return _settings;
}

int BuiltinAudioPlayer::clamp_volume(int volume) noexcept
{
    return std::clamp(volume,0,100);
}

int BuiltinAudioPlayer::to_mix_volume(int volume) noexcept
{
    return (clamp_volume(volume) * MIX_MAX_VOLUME) / 100;
}
}
