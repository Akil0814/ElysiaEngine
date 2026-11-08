# Audio Interfaces and Playback Contracts

This page maintains public parameters, results, and lifecycle contracts. See the [guide](../user-guide/systems/audio.md) for integration and [architecture](../architecture/subsystems/audio.md) for scheduling. Definitions: [audio_service.h](../../../engine/audio/audio_service.h), [sound_playback_types.h](../../../engine/audio/sound_playback_types.h), [audio_failure.h](../../../engine/audio/audio_failure.h), and [scene_sound_types.h](../../../engine/audio/scene_sound_types.h).

## Public Types and Errors

These excerpts omit namespaces, attributes, and some member defaults for reference. Include engine headers rather than duplicating these types.

```cpp
struct AudioFailure
{
    AudioError code;
    elysia::core::FailureDiagnostic diagnostic;
};
template<class T> using AudioResult = std::expected<T, AudioFailure>;

enum class SoundScope { Scene, Global };
enum class SoundPausePolicy { FollowScene, ContinueWhenPaused };
struct SpatialSoundConfig
{
    elysia::core::Vector2 position{};
    SoundDistanceConfig distance;
};
struct SoundPlayOptions
{
    std::optional<int> loops = std::nullopt;
    SoundGroup group = SoundGroup::Extra;
    std::chrono::milliseconds start_delay{0};
    std::chrono::milliseconds fade_in{0};
    double gain = 1.0;
    SoundScope scope = SoundScope::Scene;
    SoundPausePolicy pause_policy = SoundPausePolicy::FollowScene;
    std::optional<SpatialSoundConfig> spatial;
};
enum class SoundRequestStatus { Started, Scheduled };
struct SoundRequestResult
{
    SoundRequestStatus status;
    SoundHandle handle;
};
```

`SoundHandle` is a non-owning `std::uint64_t` identifier. Success contains only Started/Scheduled and a nonzero handle; failure uses `std::unexpected<AudioFailure>`. `audio_error_name` provides error-code names.

| AudioError | Meaning |
| --- | --- |
| `NotInitialized` | A device-dependent operation ran before initialization or after shutdown |
| `NoActiveScene` | The operation needs an attached scene context, but none exists |
| `InvalidArgument` | Invalid key, loops, gain, position, distance, enum, or group configuration |
| `MissingResource` | The sound or music resource is unloaded or missing |
| `InvalidHandle` | Zero, foreign, finished, or previous-lifecycle handle |
| `CooldownActive` | The resource key is within the requesting group's cooldown interval |
| `GroupLimitReached` | Group capacity is exhausted and policy cannot replace a voice |
| `ChannelLimitReached` | The logical or native track pool is exhausted |
| `BackendFailure` | Backend creation, playback, gain, pause, or stop failed |

Callers handle synchronous errors without duplicate lower-layer logging. Diagnostics retain original SDL errors, origins, and resource information. Validation or synchronous backend failure preserves parameters. Logical loop creation versus later track acquisition is described below. C++ exceptions such as allocation failure are not converted to audio error codes.

## Service Interfaces

```cpp
AudioResult<void> initialize(const AudioSettings& settings);
void shutdown() noexcept;
void update(double delta_seconds);
bool is_initialized() const noexcept;
bool scene_audio_available() const noexcept;

AudioResult<SoundRequestResult> request_sound(
    std::string_view key, const SoundPlayOptions& options = {});
AudioResult<void> stop_sound(SoundHandle handle,
    std::chrono::milliseconds fade_out = {});
AudioResult<void> set_sound_gain(SoundHandle handle, double gain);
AudioResult<void> set_sound_paused(SoundHandle handle, bool paused);
bool sound_active(SoundHandle handle);
void cancel_all_scheduled_sounds();
AudioResult<void> stop_all_sounds(std::chrono::milliseconds fade_out = {});

AudioResult<void> set_sound_group_config(SoundGroup group, const SoundGroupConfig& config);
AudioResult<SoundGroupConfig> sound_group_config(SoundGroup group) const;
AudioResult<void> set_sound_group_volume(SoundGroup group, int volume);
AudioResult<int> sound_group_volume(SoundGroup group) const;
void set_master_volume(int volume);
void set_music_volume(int volume);
void set_sound_volume(int volume);
const AudioSettings& settings() const;

AudioResult<void> play_music(std::string_view key, int loops = -1,
    std::chrono::milliseconds fade_in = {});
AudioResult<void> transition_music(std::string_view key,
    const MusicTransitionOptions& options = {});
AudioResult<void> stop_music(std::chrono::milliseconds fade_out = {});

AudioResult<void> set_listener_position(elysia::core::Vector2 position);
AudioResult<elysia::core::Vector2> listener_position() const;
AudioResult<LoopSoundId> create_loop(const LoopSoundRequest& request);
AudioResult<void> set_loop_position(LoopSoundId id, elysia::core::Vector2 position);
AudioResult<void> set_loop_gain(LoopSoundId id, double gain);
AudioResult<void> set_loop_enabled(LoopSoundId id, bool enabled);
AudioResult<void> destroy_loop(LoopSoundId id);
AudioResult<LoopSoundState> loop_state(LoopSoundId id) const;
AudioResult<void> clear_scene_sounds();
```

### `request_sound`

The single sound-request entry point has no overloads. Defaults are Scene ownership and FollowScene pause; an absent attached scene returns an error. Global explicitly selects manual lifetime and ignores scene pause policy. Global combined with spatial is invalid. Scene owns, attaches, and updates the internal runtime; gameplay does not access it directly.

Empty loops or 0 plays once; positive values add repeats; -1 loops forever; values below -1 are invalid. Nonpositive start_delay/fade_in/fade_out execute immediately. Gain accepts finite nonnegative values and is applied before native playback begins.

Started means a logical track started. Scheduled means accepted into the queue; cooldown, capacity, and backend are checked when due. A due failure logs a structured diagnostic with resource key, handle, and error code, then removes the reservation without retries or full playback history.

### Instance Gain, Pause, and Stop

set_sound_gain changes reservation gain or active base gain while preserving distance attenuation. set_sound_paused changes independent pause intent; repetition is idempotent. Pause freezes position, fades, and remaining delay while retaining channel and group slots. Instance and scene pause reasons combine.

stop_sound immediately cancels reservations and accepts stopping or fading active playback. Fade-out starts at current fade gain; repeated positive-duration stops retain the original deadline, while nonpositive durations stop immediately. Paused playback always stops immediately. Fading voices retain capacity. Completion, replacement, cancellation, or cleanup invalidates handles; scheduler resets and service reinitialization never reuse them.

sound_active returns true for reservations, playback, pause, and fade-out without guaranteeing audibility. Cleanup attempts remaining sounds and aggregates diagnostics after individual failures.

### `cancel_all_scheduled_sounds`

Only cancels all reservations and has no failure result. stop_all_sounds handles active tracks, returns `AudioResult<void>`, and leaves reservations intact. clear_scene_sounds cleans only the current scene's tracks, reservations, and logical sources, preserving listener and scene pause state.

## Concurrency Groups, Cooldown, and Overflow

| SoundGroup | Hard limit |
| --- | ---: |
| Ui | 4 |
| Gameplay | 12 |
| Ambient | 4 |
| Extra | 4 |

The total is 24 sound channels, excluding music. Other idle groups cannot lend capacity. SoundGroupConfig contains optional max_simultaneous (empty uses the hard limit, explicit values may only lower it), nonnegative millisecond cooldown, and overflow_policy (IgnoreNew/ReplaceOldest). Group identifiers and enums must be valid.

Cooldown records the latest successful start per resource key, applying the current request group's duration. A key shares history across groups; failures do not update it. Cooldown is checked before replacement. IgnoreNew rejects a full group; ReplaceOldest stops its oldest active instance before starting the new sound, without preempting other groups. The new start may still fail after the previous voice has stopped.

Lowering capacity does not stop existing voices. ReplaceOldest replaces at most one per request rather than immediately converging an already over-limit group. Explicit zero always rejects new sounds, including replacement. Invalid configuration preserves old values. Group queries return copies and do not require device initialization.

## Volume Model

AudioSettings master_volume, music_volume, and sound_volume all default to 100. Their void setters clamp to 0–100 and log internal application failures. settings() returns a read-only borrowed reference; copy it for a snapshot.

Group volume defaults to 100 and clamps to 0–100. Its expected setter validates the group and restores the previous group parameter on backend failure. Group configuration and volume queries return validated snapshots. Group rules and volume are runtime-only and do not persist automatically.

Final sound gain is `(master/100) * (sound/100) * (group/100) * instance * fade * distance`; ordinary sounds use distance=1. Music uses `(master/100) * (music/100) * fade`. Settings changes preserve instance and fade gains. Finite double gains are limited to representable SDL float values before submission.

## Music API

play_music validates and restarts playback, clearing pending targets on success; even the same key restarts. transition_music fades out the old track, starts the target, then fades it in. MusicTransitionOptions contains loops=-1, fade_out=0, and fade_in=0.

Changing targets during fade-out updates the pending key, loops, and fade-in duration while preserving the old fade deadline. Requesting the currently playing key cancels pending transition, preserves position and loops, and recovers full gain. stop_music clears the target; repeated positive stops retain fade timing. Switching while idle starts immediately; a naturally finished track starts the target on the next update.

Synchronous failure preserves control parameters and accepted targets. A replacement track is prepared successfully before replacing current playback. Future startup looks up the resource again; failure logs a diagnostic, clears the target, and becomes idle without retry. Music remains a global single-track sequence outside sound budgets, reservations, and handles.

## Scene Sound Source Interfaces

Every Scene owns an audio runtime. SceneRuntimeFeatures.audio is a plain SceneAudioConfig; loop_retry_interval_seconds defaults to 0.25 and must be finite and at least 0.25. Invalid construction configuration throws `std::invalid_argument`. The listener is queryable during attachment and defaults to the origin. Its setter validates parameters; track updates follow on_after_update in the next scene update.

SoundDistanceConfig explicitly supplies positive reference_distance, cutoff_distance at least reference, release_distance at least cutoff, and positive exponent. All values and positions must be finite. Gain is 1 within reference, then `(reference_distance / distance)^exponent`, and zero beyond cutoff. Units are EU; coordinate differences use double precision.

LoopSoundRequest contains key, position, distance, gain=1, group=Ambient, enabled=true, and pause_policy=FollowScene. create_loop returns a logical-source handle even if resources or channels are temporarily unavailable. LoopSoundState is a snapshot with status, optional voice, and optional last_error. voice is observational; control persistent sources through loop methods.

Statuses are Playing, Disabled, ScenePaused, Inaudible, OutOfRange, WaitingForVoice, and MissingResource. OutOfRange takes precedence over disable and scene pause. Between cutoff and release, paused tracks are retained; beyond release the track stops while the logical source remains. Audible return restarts playback. Acquisition follows existing budgets, cooldown, overflow rules, and retry intervals. Missing resources do not log each frame. Acquiring a voice or no longer needing playback clears last_error.

set_loop_position/gain/enabled validate before committing; synchronous backend failure restores original request parameters and returns an error. Temporary resource or capacity failures remain logical state with last_error. destroy_loop and clear_scene_sounds are cleanup operations: partial stop failure still retires logical sources and continues remaining cleanup. Infrastructure cleanup logs aggregated failures.

## Lifecycle and Capability Boundaries

The application drives initialize, update, and shutdown. Initialization failure enters the startup diagnostic path. Shutdown/reinitialization stops active sounds, clears reservations, loops, and music state. A surviving scene binding remains valid, but old sources are not restored. ResourceService has_sound/has_music are quiet existence queries; unloading resources stops and detaches associated native tracks.

Scenes bind before user attachment/entry and unbind before detach/exit. Exit, reuse/reset, entry failure, attachment rollback, and destruction clean owned sounds even when user callbacks throw. ContinueWhenPaused only exempts pause. Global sounds and music remain caller-owned.

The current facility provides one listener and distance gain. Panning, occlusion, Doppler, multiple listeners, cross-group preemption, global capacity-wait queues, and simultaneous music crossfade are outside this version. See the [guide](../user-guide/systems/audio.md) for migration mappings.
