# Audio Playback

This document covers resource queries/playback, handles, delayed requests, and volume. Resource existence, request acceptance, and actual playback are distinct states.

Use `ELYSIA_AUDIO` for loaded sounds/music. The application initializes, updates, and closes it; games should not repeat lifecycle calls. Declare Sound/Music in [resource manifests](../content/resources.md) and wait for loading success.

## Playing and Managing a Sound

Call this game function from input or game events. Configure its keys in the `audio` manifest and retain the returned handle in its skill/scene owner.

```cpp
#include "engine/audio/audio_service.h"
#include "engine/tools/logger.h"

#include <optional>
#include <chrono>

std::optional<elysia::audio::SoundHandle> play_explosion()
{
    using namespace std::chrono_literals;
    // Application scheduling advances pending sounds; handles do not imply audible playback.
    const elysia::audio::SoundRequestResult result = ELYSIA_AUDIO->request_sound("skill.explosion", {
        .group = elysia::audio::SoundGroup::Gameplay,
        .start_delay = 300ms
    });
    if (result.status == elysia::audio::SoundRequestStatus::Rejected)
    {
        ELYSIA_LOG_WARN("audio", "Explosion sound rejected.");
        return std::nullopt;
    }
    // Retain handles and explicitly cancel/stop on skill cancellation or scene exit.
    return result.handle;
}

// Call before skill cancellation, scene exit, or owner destruction.
void cancel_sound(std::optional<elysia::audio::SoundHandle>& handle)
{
    if (!handle)
        return;
    // false can also mean natural completion; no repeated stop is needed.
    (void)ELYSIA_AUDIO->stop_sound(*handle);
    handle.reset();
}
```

The service does not automatically clean requests by scene/object. Handles extend neither resource nor object lifetime. Scheduled means accepted scheduling, not guaranteed future playback.

## UI Sounds and Group Rules

This fragment belongs in initialized game logic with `audio`/log headers. `audio` is borrowed from `elysia::audio::AudioService::instance()`; millisecond literals use `std::chrono_literals`. The project's manifest must supply the example keys.

```cpp
const elysia::audio::SoundRequestResult click = audio->request_sound("ui.button_click", {
    .group = elysia::audio::SoundGroup::Ui
});
if (click.status == elysia::audio::SoundRequestStatus::Rejected)
{
    // Failed feedback sounds do not block UI; follow project logging policy.
    ELYSIA_LOG_WARN("audio", "UI click sound rejected.");
}
```

High-frequency skills can replace the group's oldest sound. Set rules before requesting playback; failed configuration retains old rules.

```cpp
elysia::audio::SoundGroupConfig gameplay_config{};
gameplay_config.max_simultaneous = 8;
gameplay_config.cooldown = 20ms;
gameplay_config.overflow_policy = elysia::audio::SoundOverflowPolicy::ReplaceOldest;
if (!audio->set_sound_group_config(elysia::audio::SoundGroup::Gameplay, gameplay_config))
{
    ELYSIA_LOG_WARN("audio", "Gameplay sound configuration rejected.");
}
```

See [concurrency rules](../../reference/audio.md#concurrency-groups-cooldown-and-overflow) for hard limits, per-resource cooldown, and replacement boundaries. Do not bypass rejection with unlimited retries.

## Ambient Volume and Music Switching

Use group volume to lower ambience without affecting music/skills. This fragment uses `audio` from above:

```cpp
audio->set_sound_group_volume(elysia::audio::SoundGroup::Ambient, 45);
```

Group volume is runtime-only; persist preferences through [user configuration](../content/configuration.md). See [volume model](../../reference/audio.md#volume-model) for ranges/final gain.

The following fades old music out before fading battle music in and starts looping rain. It belongs in host entry logic. `rain_handle` is an initially empty member `std::optional<elysia::audio::SoundHandle>`; clean the old handle before reentry.

```cpp
if (!audio->transition_music("bgm.battle", {
        .fade_out = 800ms,
        .fade_in = 500ms}))
{
    // Validation preserves state on failure; synchronous startup failure may have stopped old music.
    ELYSIA_LOG_WARN("audio", "Battle music transition rejected.");
}
const elysia::audio::SoundRequestResult rain = audio->request_sound("ambient.rain", {
    .loops = -1,
    .group = elysia::audio::SoundGroup::Ambient,
    .fade_in = 1000ms
});
rain_handle = rain.handle;
if (rain.status == elysia::audio::SoundRequestStatus::Rejected)
{
    ELYSIA_LOG_WARN("audio", "Rain sound rejected.");
}
```

On leaving this flow, run the following. After submitting fade-out, retaining the handle is unnecessary and clearing it prevents reuse on reentry.

```cpp
if (rain_handle)
{
    (void)audio->stop_sound(*rain_handle, 800ms);
    rain_handle.reset();
}
```

Successful switching means request acceptance only; future startup can still fail. See [music API](../../reference/audio.md#music-api). The current single music channel does not support simultaneous crossfading of two tracks.

## Cleanup and Failure Diagnosis

The application advances `audio` after scene updates. Delay, cooldown, and fades follow global time scale; scene pause does not pause global sound. Slow motion does not automatically change pitch/speed of started tracks.

Check initialization and keys first, then distinguish rejection, accepted scheduling, and actual playback. Due requests can still drop for cooldown, concurrency, or backend failure. Playback return values are not completion notifications.

Stop owned handles before scene exit, skill cancellation, or host destruction. Clearing all global sounds requires handling active playback and pending requests separately; see [stopping and cancellation](../../reference/audio.md#cancel_all_scheduled_sounds).

## Controlling Instance Gain and Pause

`SoundPlayOptions::gain` is a nonnegative instance multiplier independent of player volume, defaulting to 1. Retain the returned handle and use `set_sound_gain` to change it, or `set_sound_paused(handle, true/false)` to pause/resume. `sound_active` includes reservations, paused voices and fades; it does not establish that a sound is currently audible.

Pausing freezes playback position and fade progress; an unstarted reservation freezes its remaining delay. Stopping a paused sound releases it immediately without requiring resume. The application still updates the global service; games must not advance it a second time.

## Giving Sounds to a Scene

Enable `SceneRuntimeFeatures::audio` for automatic pause and cleanup, then call `audio_runtime()` from the derived Scene. Use `request_sound` for ordinary owned sounds, `request_sound_at` for world-positioned sounds, and `create_loop` for persistent loops with distance-based sleeping and restart. The first two return normal playback results; loops return `LoopSoundId`.

This complete class requires an already loaded `ambient.wind` sound in the manifest. Positions and distances use EU. The game updates the listener explicitly; it does not automatically follow a camera. These example distances are project choices, not engine defaults.

```cpp
#include "engine/scene/scene.h"

class WindScene final : public elysia::scene::Scene
{
public:
    WindScene() : Scene(elysia::scene::SceneRuntimeFeatures{
        .audio = elysia::scene::SceneAudioConfig{}}) {}

    void move_listener(elysia::core::Vector2 position)
    {
        (void)audio_runtime().set_listener_position(position);
    }

protected:
    void on_enter(const elysia::scene::ScenePayload&) override
    {
        (void)audio_runtime().set_listener_position({0, 0});
        wind = audio_runtime().create_loop({
            .key = "ambient.wind",
            .position = {100, 0},
            .distance = {50, 400, 480, 1.4},
            .gain = 0.5
        });
    }
    void on_exit() override { wind.reset(); }
    void on_reset() override { wind.reset(); }

private:
    std::optional<elysia::scene::LoopSoundId> wind;
};
```

The runtime refreshes distances after `on_after_update`. Positional one-shots continuously adjust gain and never restart after finishing. A loop pauses and preserves position beyond its audible cutoff of 400, then releases its channel beyond 480; returning to audible range starts playback from the beginning. `set_loop_enabled` and scene pause are tracked separately, so scene resume never enables a disabled source.

Sounds follow scene pause by default; UI feedback can pass `SceneSoundPausePolicy::ContinueWhenPaused` to `request_sound`. The runtime clears only its own sounds, leaving other scenes, global sounds and music alone. Exit, reset, failed entry and destruction clear ownership automatically; recreate sources in `on_enter` when reusing a scene.

`loop_state` is a snapshot after the last update or source setter and never starts playback. `WaitingForVoice` means no channel has been acquired; acquisition retries at most once every 250 ms. `MissingResource` reports a missing asset, with one diagnostic per missing episode. The total remains 24 channels with at most 4 actual Ambient tracks by default; logical sources reserve no extra channels.

## Integrating FlyMe2TheMoon

The project's initial gain, `set_sound_gain`, `set_sound_paused` and `sound_active` map directly to public service interfaces. Replace `World::set_listener` with the scene listener position, map `World::at` to `request_sound_at`, and replace owner-indexed loops with game-owned `LoopSoundId` values, updating their positions, gains and enabled states.

Convert project distance models into `SoundDistanceConfig`. Character voices, crystal-chain rules, resource aliases and original parameters remain in the game. Remove duplicate manual pause/cleanup paths for migrated sounds; the project still chooses when to stop global music. This change does not modify FlyMe2TheMoon or expand engine channel budgets.

## Further Reading

- [Audio interfaces and playback contracts](../../reference/audio.md): full parameters, results, boundaries.
- [Audio architecture](../../architecture/subsystems/audio.md): scheduling order, track backend, fades.
- [Resource loading](../content/resources.md), [back to user guide](../README.md).
