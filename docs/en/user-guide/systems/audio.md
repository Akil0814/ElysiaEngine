# Audio Playback

Use `ELYSIA_AUDIO` to configure volume, request sounds, and control sources. Ordinary requests belong to the attached scene by default: exiting, resetting, or destroying that scene cleans up its sounds and reservations. Spatial sounds use the same entry point with a complete position and distance configuration.

The application initializes, updates, and shuts down audio. Declare sounds and music in the [resource manifest](../content/resources.md), wait for content loading, then request playback from scene entry, update, or input callbacks.

## Requesting Sounds from a Scene

Every `Scene` owns an audio runtime by default; no enable switch is needed. `scene_audio_available()` indicates an attached scene context; `is_initialized()` indicates device-service readiness. Normal SceneManager flow binds before the user attach callback and `on_enter`, and unbinds before detach and `on_exit`. Without a context, default requests return `NoActiveScene` rather than changing ownership.

This complete scene requires loaded `ambient.wind`, `skill.explosion`, and `ui.prompt` resources. The example distances are project choices in EU. The game explicitly updates the listener; it does not automatically follow the camera.

```cpp
#include "engine/scene/scene.h"
#include "engine/audio/audio_service.h"
#include "engine/tools/logger.h"
#include <optional>
#include <chrono>

class WindScene final : public elysia::scene::Scene
{
public:
    void move_listener(elysia::core::Vector2 position)
    {
        check(ELYSIA_AUDIO->set_listener_position(position));
    }

    void explosion(elysia::core::Vector2 position)
    {
        auto result = ELYSIA_AUDIO->request_sound("skill.explosion", {
            .group = elysia::audio::SoundGroup::Gameplay,
            .gain = 0.8,
            .spatial = elysia::audio::SpatialSoundConfig{
                position, {50, 400, 480, 1.4}}
        });
        if (!result) report(result.error());
    }

    void ui_prompt()
    {
        auto result = ELYSIA_AUDIO->request_sound("ui.prompt", {
            .group = elysia::audio::SoundGroup::Ui,
            .pause_policy = elysia::audio::SoundPausePolicy::ContinueWhenPaused
        });
        if (!result) report(result.error());
    }

protected:
    void on_enter(const elysia::scene::ScenePayload&) override
    {
        auto result = ELYSIA_AUDIO->create_loop({
            .key = "ambient.wind",
            .position = {100, 0},
            .distance = {50, 400, 480, 1.4},
            .gain = 0.5
        });
        if (result) wind = *result;
        else report(result.error());
    }
    void on_exit() override { wind.reset(); }
    void on_reset() override { wind.reset(); }

private:
    static void report(const elysia::audio::AudioFailure& failure)
    {
        ELYSIA_LOG_WARN("game.audio",
            elysia::audio::audio_error_name(failure.code) << ": "
            << failure.diagnostic.message);
    }
    static void check(const elysia::audio::AudioResult<void>& result)
    {
        if (!result) report(result.error());
    }
    std::optional<elysia::audio::LoopSoundId> wind;
};
```

A successful `request_sound` supplies `Started` or `Scheduled` and a nonzero handle. On failure, inspect `result.error().code` and `diagnostic`. Acceptance is not playback completion; a scheduled request can still fail when it becomes due.

Omit `spatial` in `SoundPlayOptions` for an ordinary sound. When supplied, configure all distance fields: gain is 1 within the reference distance, then falls as `(reference_distance / distance)^exponent`, and is 0 beyond cutoff. Position and distances must be finite, reference distance positive, cutoff at least reference, release at least cutoff, and exponent positive.

Positional one-shots update gain as the listener moves, retire naturally, and never replay because distance changes. Persistent spatial ambience uses `create_loop`: beyond cutoff it pauses while preserving position; beyond release it frees the channel. Returning to audible range restarts playback from the beginning.

## Controlling Sounds and Pause

Save `result->handle` to use `set_sound_gain`, `set_sound_paused`, and `stop_sound`. Instance gain accepts finite nonnegative values and multiplies master, sound, group, and fade gain. Global volume changes preserve instance gain; the service also preserves owned base gain and independent pause intent.

Scene sounds follow scene pause by default. The UI example uses `SoundPausePolicy::ContinueWhenPaused`: it remains scene-owned but plays during pause. Instance and scene pause reasons combine; resuming a scene neither resumes a manually paused sound nor enables a disabled loop. Global sounds ignore scene pause policy.

Pause freezes playback position, fades, and remaining reservation delay; paused tracks retain channels and group slots. Stopping a paused sound releases it immediately, even with a positive fade duration. `sound_active` includes reservations, pause, and fade-out, so it does not imply audibility. Completed, replaced, or cleaned-up handles are invalid; service restarts never reuse old handles.

Loops have separate `LoopSoundId` handles. Modify them with `set_loop_position`, `set_loop_gain`, and `set_loop_enabled`; query snapshots with `loop_state` and destroy with `destroy_loop`. Successful creation means a logical source exists. `WaitingForVoice`, `MissingResource`, and `last_error` explain why it has no track. Acquiring a voice or no longer needing playback clears the error; an unchanged failure does not log every frame. Capacity retries occur at most once per 250 ms by default; `SceneRuntimeFeatures::audio` accepts `SceneAudioConfig` to increase that interval.

## Global Sounds, Group Rules, and Music

Set `scope = SoundScope::Global` for sounds that must survive scenes or play without a scene, and stop them explicitly. Built-in button, checkbox, radio-button, and slider feedback uses this ownership. `Global + spatial` returns `InvalidArgument` because the listener belongs to a scene. Ordinary UI prompts can also retain default scene ownership with a chosen pause policy.

Configure groups and group volumes through the service and check `AudioResult<void>`; queries return value snapshots. There are still 24 sound channels: Ui 4, Gameplay 12, Ambient 4, Extra 4. See the [reference](../../reference/audio.md) for cooldown and overflow rules. Do not bypass ordinary-request capacity failures with frame-by-frame retries.

Master, music, and sound volume setters retain a non-failing 0–100 clamping contract. Group volume also validates its group and submits to the backend. Group configuration is runtime-only; persist player preferences through [user configuration](../content/configuration.md).

`play_music`, `transition_music`, and `stop_music` return `AudioResult<void>`. Music remains a global single-track facility: transitions fade out then fade in, without simultaneous crossfade. Synchronous validation or startup failure preserves controller state; a later startup failure ends the accepted transition and records a diagnostic.

## Failure and Cleanup

Callers handle or explicitly ignore synchronous failures; lower layers do not log them again. `AudioError` distinguishes initialization, scene context, parameters, resources, handles, cooldown, group capacity, total channels, and backend failures. See the [reference](../../reference/audio.md) for exact results and contracts. Due reservations log their key, handle, and error code on failure, then retire without unlimited retries or a full playback history.

Distance and source state refresh at the end of scene update, after `on_after_update`. The application advances global scheduling after scenes. Time scaling affects delays, cooldown, and fades, without changing native playback speed or pitch. Game code must not update the service again.

Exit, reuse/reset, entry failure, runtime detach, and destruction clean up owned tracks, reservations, and logical sources, including when user lifecycle callbacks throw. `clear_scene_sounds()` clears the current scene while preserving listener and pause state. Global sounds and music remain manually managed: `stop_all_sounds` handles active tracks, while `cancel_all_scheduled_sounds` separately cancels all reservations. Cleanup attempts every sound and aggregates diagnostics.

## Migrating Old Interfaces and FlyMe2TheMoon

This is a breaking update without deprecated wrappers. Review old `request_sound` calls in particular: previously manual global requests now default to the current scene.

| Previous usage | Replacement |
| --- | --- |
| `play_sound(key, loops)` | `request_sound(key, {.loops = loops})`, inspect expected |
| Old global `request_sound` behavior | Explicit `.scope = SoundScope::Global` |
| `request_scene_sound` | Default `request_sound` |
| `request_sound_at` / `World::at` | `.spatial = SpatialSoundConfig{position, distance}` in `request_sound` |
| `SceneSoundPausePolicy` | `SoundPausePolicy` in options or loop requests |
| `Rejected` / optional result handle | `!result` / `result.error()`; `result->handle` on success |
| `bool` command results | `AudioResult<void>` with details in `error()` |
| `has_audio` / `try_audio_runtime` / `audio_runtime` | Removed; query `scene_audio_available()` on the service |
| Optional scene-audio enablement | Always provided; retain `SceneAudioConfig` for tuning |

This FlyMe2TheMoon adapter returns ordinary handles for its existing cancellation flow. Optional global UI feedback explicitly discards the result.

```cpp
#include "engine/audio/audio_service.h"

elysia::audio::AudioResult<elysia::audio::SoundHandle> play_world_at(
    std::string_view key, elysia::core::Vector2 position,
    const elysia::audio::SoundDistanceConfig& distance)
{
    auto result = ELYSIA_AUDIO->request_sound(key, {
        .group = elysia::audio::SoundGroup::Gameplay,
        .spatial = elysia::audio::SpatialSoundConfig{position, distance}
    });
    if (!result) return std::unexpected(result.error());
    return result->handle;
}

void optional_global_click()
{
    (void)ELYSIA_AUDIO->request_sound("ui.click", {
        .group = elysia::audio::SoundGroup::Ui,
        .scope = elysia::audio::SoundScope::Global
    });
}
```

Map `World::set_listener` to `set_listener_position`; replace owner-managed loops with saved `LoopSoundId` values and position/gain/enabled updates. Remove duplicate manual pause and cleanup for the same scene-owned sound. Convert project distance models to `SoundDistanceConfig`. Character voices, crystal chains, resource keys, and concrete tuning remain game concerns. FlyMe2TheMoon was not modified in this update.

## Further Reading

- [Audio interfaces and playback contracts](../../reference/audio.md)
- [Audio architecture](../../architecture/subsystems/audio.md)
- [Resource loading](../content/resources.md), [guide index](../README.md)
