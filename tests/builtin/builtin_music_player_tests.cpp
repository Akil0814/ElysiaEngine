#define SDL_MAIN_HANDLED
#include "tests/support/sdl_audio_fixture.h"
#include "tests/support/test_assertions.h"
#include "engine/builtin/resources/builtin_resources.h"
#include "engine/audio/audio_service.h"
#include "engine/loading/content_runtime_cleanup.h"
#include "engine/resources/runtime/resource_manager.h"
#include "engine/resources/resource_service.h"

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <array>
#include <cmath>
#include <filesystem>
#include <vector>

namespace
{
using elysia::tests::require;
using namespace std::chrono_literals;
using elysia::builtin::BuiltinMusicId;
constexpr auto kMusic = BuiltinMusicId::ElysianRealm;
constexpr int kFrequency = 48000;
constexpr int kChannels = 2;

void reset_offline_mixer()
{
    elysia::builtin::BuiltinMusicPlayer::instance()->shutdown();
    auto& backend = elysia::audio::detail::mixer_backend();
    backend.shutdown();
    const SDL_AudioSpec spec{.format = SDL_AUDIO_F32, .channels = kChannels, .freq = kFrequency};
    backend.mixer = MIX_CreateMixer(&spec);
    require(backend.mixer != nullptr, "offline mixer is created without a playback device");
    backend.music = MIX_CreateTrack(backend.mixer);
    require(backend.music != nullptr, "project music track is created");
    for (auto& track : backend.tracks)
    {
        track = MIX_CreateTrack(backend.mixer);
        require(track != nullptr, "project sound track is created");
    }
}

std::vector<float> generate(double seconds)
{
    std::vector<float> result(static_cast<std::size_t>(seconds * kFrequency) * kChannels);
    const int bytes = static_cast<int>(result.size() * sizeof(float));
    require(MIX_Generate(elysia::audio::detail::mixer_backend().mixer,
        result.data(), bytes) >= 0, "offline mixer successfully fills the requested output buffer");
    return result;
}

double ratio(const std::vector<float>& output, const std::vector<float>& baseline,
    double begin, double end)
{
    double cross = 0;
    double energy = 0;
    const auto first = static_cast<std::size_t>(begin * kFrequency) * kChannels;
    const auto last = static_cast<std::size_t>(end * kFrequency) * kChannels;
    for (auto i = first; i < last; ++i)
    {
        cross += output[i] * baseline[i];
        energy += baseline[i] * baseline[i];
    }
    require(energy > 1e-6, "reference music window contains audible samples");
    return cross / energy;
}

void test_fade()
{
    auto& player = *elysia::builtin::BuiltinMusicPlayer::instance();
    require(player.initialize({}), "offline player initializes");
    require(player.play(kMusic), "reference music starts immediately");
    auto baseline = generate(0.75);
    const auto baseline_tail = generate(1.25);
    baseline.insert(baseline.end(), baseline_tail.begin(), baseline_tail.end());

    // Use fresh streams and identical chunk boundaries for sample-aligned captures.
    reset_offline_mixer();
    require(player.initialize({}), "player retries on a fresh offline mixer");
    require(player.play(kMusic, -1, 1500ms), "same music restarts with native fade");
    auto faded = generate(0.75);
    player.set_master_volume(50);
    const auto tail = generate(1.25);
    faded.insert(faded.end(), tail.begin(), tail.end());
    require(ratio(faded, baseline, 0.1, 0.3) > 0.06
        && ratio(faded, baseline, 0.1, 0.3) < 0.22,
        "native fade output starts quietly");
    require(ratio(faded, baseline, 0.6, 0.7) > 0.39
        && ratio(faded, baseline, 0.6, 0.7) < 0.48,
        "native fade output rises before the setting change");
    require(ratio(faded, baseline, 0.9, 1.1) > 0.28
        && ratio(faded, baseline, 0.9, 1.1) < 0.38,
        "setting change multiplies the ongoing fade without restarting it");
    require(std::abs(ratio(faded, baseline, 1.6, 1.8) - 0.5) < 0.01,
        "1500ms fade completes at the current master volume");

    player.set_music_volume(-10);
    for (float sample : generate(0.1))
        require(sample == 0, "zero music volume immediately silences mixed output");
    player.set_master_volume(125);
    player.set_music_volume(125);
    require(MIX_GetTrackGain(elysia::builtin::BuiltinMusicPlayerTestAccess::track()) == 1,
        "volume settings clamp to 100");
    reset_offline_mixer();
    require(player.initialize({}), "player initializes for immediate output capture");
    require(player.play(kMusic, -1, -1ms), "negative fade plays immediately");
    auto immediate = generate(0.75);
    const auto immediate_tail = generate(1.25);
    immediate.insert(immediate.end(), immediate_tail.begin(), immediate_tail.end());
    require(std::abs(ratio(immediate, baseline, 0.1, 0.3) - 1) < 0.01,
        "nonpositive fade does not attenuate actual output");
}

void test_isolation_and_lifetime(SDL_Renderer* renderer,
    const elysia::builtin::BuiltinAssetCatalog& catalog)
{
    auto& player = *elysia::builtin::BuiltinMusicPlayer::instance();
    auto& resources = *elysia::builtin::BuiltinResources::instance();
    auto* track = elysia::builtin::BuiltinMusicPlayerTestAccess::track();
    require(player.initialize({.master_volume = 50, .music_volume = 50}),
        "repeat initialization updates volume");
    require(track == elysia::builtin::BuiltinMusicPlayerTestAccess::track()
        && MIX_GetTrackGain(track) == 0.25f && MIX_TrackPlaying(track),
        "repeat initialization preserves the live track and playback");
    require(!player.play(static_cast<BuiltinMusicId>(255))
        && !player.play(BuiltinMusicId::Count) && MIX_TrackPlaying(track),
        "invalid IDs preserve current playback");

    auto* project = elysia::resources::ResourceManager::instance();
    require(!elysia::resources::ResourceService::instance()->find_music(
        elysia::builtin::builtin_resource_name(kMusic)),
        "public pool cannot find built-in music");
    const std::filesystem::path root = ELYSIA_SOURCE_DIR;
    require(project->load_music(elysia::resources::MusicLoadRequest{
        "project.test", root / "assets/engine/audio/Elysian_Realm.ogg", {}}).has_value(),
        "project loads its own independent music instance");
    auto& audio = *elysia::audio::AudioService::instance();
    require(audio.initialize({}) && audio.play_music("project.test"), "project music starts");
    require(track != elysia::audio::detail::mixer_backend().music
        && elysia::tests::builtin_music_playing() && elysia::tests::music_playing(),
        "project and built-in music have independent playing tracks");
    audio.set_master_volume(10);
    audio.set_music_volume(10);
    audio.stop_music();
    require(MIX_TrackPlaying(track) && MIX_GetTrackGain(track) == 0.25f,
        "project volume and stop do not affect the built-in track");
    require(audio.play_music("project.test"), "project music restarts");
    player.stop();
    require(!MIX_TrackPlaying(track) && !MIX_GetTrackAudio(track)
        && elysia::tests::music_playing(), "built-in stop leaves project playback alone");
    require(player.play(kMusic), "built-in music restarts");
    elysia::loading::clear_loaded_content();
    audio.shutdown();
    require(MIX_TrackPlaying(track), "project clear and shutdown preserve built-in playback");

    require(!resources.initialize(renderer,
        elysia::builtin::BuiltinAssetCatalog(root / "missing_builtin_root"), std::array{20}),
        "failed resource reinitialization returns an error");
    require(!MIX_GetTrackAudio(track) && resources.is_initialized(),
        "even failed reinitialization stops playback while preserving published assets");
    require(player.play(kMusic), "retained resource remains playable");
    require(resources.initialize(renderer, catalog, std::array{20}).has_value(),
        "successful resource reinitialization replaces assets");
    require(!MIX_GetTrackAudio(track), "resource replacement detaches old music");
    require(player.play(kMusic), "replaced music is playable");
    resources.shutdown();
    require(!MIX_GetTrackAudio(track) && !MIX_TrackPlaying(track),
        "resource shutdown stops and detaches music before release");
    require(!player.play(kMusic), "missing resource does not fall back to project pool");
    player.shutdown();
    player.shutdown();
    require(!player.is_initialized(), "repeated shutdown leaves an empty player");
    require(player.initialize({}), "player reinitializes after shutdown");
    player.shutdown();
}
}

int main()
{
    auto& player = *elysia::builtin::BuiltinMusicPlayer::instance();
    require(!player.is_initialized() && !player.play(kMusic),
        "singleton access creates an empty player and uninitialized play fails");
    require(!player.initialize({}) && !player.is_initialized(),
        "initialization before MIX_Init fails without publishing a track");

    SDL_setenv_unsafe("SDL_AUDIO_DRIVER", "dummy", 1);
    require(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) && TTF_Init() && MIX_Init(),
        "SDL, TTF and mixer initialize");
    auto& backend = elysia::audio::detail::mixer_backend();
    reset_offline_mixer();
    auto* surface = SDL_CreateSurface(16, 16, SDL_PIXELFORMAT_RGBA32);
    require(surface != nullptr, "software surface is created");
    auto* renderer = SDL_CreateSoftwareRenderer(surface);
    require(renderer != nullptr, "software renderer is created");
    const elysia::builtin::BuiltinAssetCatalog catalog(std::filesystem::path{ELYSIA_SOURCE_DIR});
    require(elysia::builtin::BuiltinResources::instance()->initialize(
        renderer, catalog, std::array{20}).has_value(), "built-in resources initialize");
    test_fade();
    test_isolation_and_lifetime(renderer, catalog);
    SDL_DestroyRenderer(renderer);
    SDL_DestroySurface(surface);
    backend.shutdown();
    MIX_Quit();
    TTF_Quit();
    SDL_Quit();
}
