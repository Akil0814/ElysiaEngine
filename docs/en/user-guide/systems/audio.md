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

## Further Reading

- [Audio interfaces and playback contracts](../../reference/audio.md): full parameters, results, boundaries.
- [Audio architecture](../../architecture/subsystems/audio.md): scheduling order, track backend, fades.
- [Resource loading](../content/resources.md), [back to user guide](../README.md).
