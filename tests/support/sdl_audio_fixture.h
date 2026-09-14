#pragma once
#include "engine/audio/mixer_backend.h"
namespace elysia::tests
{
inline bool open_test_mixer()
{
    return MIX_Init() && audio::detail::mixer_backend().initialize();
}
inline void close_test_mixer()
{
    audio::detail::mixer_backend().shutdown();
    MIX_Quit();
}
inline bool music_playing()
{
    auto* track = audio::detail::mixer_backend().music;
    return track && MIX_TrackPlaying(track);
}
inline bool sound_playing(int channel)
{
    auto* track = audio::detail::mixer_backend().tracks.at(channel);
    return track && MIX_TrackPlaying(track);
}
inline float music_gain() { return MIX_GetTrackGain(audio::detail::mixer_backend().music); }
inline float sound_gain(int channel) { return MIX_GetTrackGain(audio::detail::mixer_backend().tracks.at(channel)); }
}
