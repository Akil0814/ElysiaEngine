# Audio Architecture and Scheduling

This page explains cooperation between resource ownership, request scheduling, tracks, and fades for audio maintainers. Game integration is in the [audio guide](../../user-guide/systems/audio.md); complete public contracts are in the [reference](../../reference/audio.md).

## Module Boundaries

Audio has three layers: resource ownership, runtime scheduling, and backend playback.

```text
Audio manifest / content loading
        │
        ▼
ResourceManager / AudioManager
  Own MIX_Audio by key (separate sound/music indexes)
        │
        ▼
AudioService
  Validate resources, schedule, control volume, call SDL_mixer
        │
        ▼
SDL_mixer
  MIX_Track / MIX_PlayTrack / MIX_StopTrack
```

- `ResourceManager` finds loaded `MIX_Audio` by key (predecoded sounds, music decoded on demand) without deciding playback policy.
- `AudioService` is the unified gameplay/UI entry point, owning the global scheduler and borrowing the attached SceneAudioRuntime for scene requests. It does not own Scene objects.
- Internal `SoundPlaybackScheduler` manages pending/active sounds, `SoundHandle`s, concurrency groups, cooldowns, and overflow policy.
- SDL3_mixer plays tracks. Scheduler channels are engine logical indices; resource lookup and track operations are adapted by the service/backend, not SDL2_mixer channel APIs.

## Lifecycle and Frame Update

The application calls `MIX_Init`, then checks `AudioResult<void>` from `AudioService::initialize` and forwards its diagnostic to startup failure handling, stopping dependent initialization on failure. Through shared `MixerBackend`, the service creates or reuses the device, sound tracks, and independent music track, resets scheduling/group volumes, and applies initial volume. Reinitialization first clears the previous lifecycle's state.

Each frame, after scene update and switching, the application calls `AudioService::update` with scaled delta. The service advances `SoundPlaybackScheduler` before `MusicPlaybackController`; negative or non-finite delta becomes zero. Due reservations and transition targets start after existing fades are processed; their fade-in does not consume that update's delta again. The device independently plays started tracks, so scheduling-clock changes do not change track playback speed automatically.

Shutdown stops all tracks and clears reservations, active instances, music targets, fades and attached scene sources without waiting for fade-out. The application then clears resources and shuts down the shared backend to destroy tracks and device. Global requests remain caller-managed; scene requests are cleaned up through their owning runtime.

## Sound Scheduling and Music State Machine

`SoundPlaybackScheduler` is not a direct gameplay/UI entry point. It:

- Creates and maintains `SoundHandle`s, retaining each handle from Pending to Playing.
- Tracks reservation due times and active logical channels.
- Reaps SDL-completed channels when starting, stopping, or updating sounds, preventing stale concurrency counts.
- Checks cooldowns, group/global limits, and overflow policy.
- For `ReplaceOldest`, selects the group's oldest active sound and stops its logical channel through a callback.
- Stores instance fade/stopping state and enumerates channels/gains by group for `AudioService` volume updates.

`AudioService` supplies start, playing-state, and stop callbacks. Start receives initial gain; update applies gain. Internal `MusicPlaybackController` likewise uses playback, stop, state, and volume callbacks without caching resource pointers. Shared `AudioFade` provides linear interpolation. Scheduling therefore depends on neither `Scene`, `ResourceManager`, nor SDL resource lookup; SDL details stay in the service layer.

## Gain Calculation and Track Playback

Schedulers manage instance fade coefficients; `AudioService` multiplies them by user and group volume. Music and sound use floating-point `MIX_SetTrackGain`; formulas are maintained only in the [volume reference](../../reference/audio.md#volume-model).

Set initial gain before starting a track to avoid a full-volume first frame. Music replacement prepares a temporary track with its initial gain and starts it successfully before committing the new music track pointer and destroying the previous track, preserving current playback on synchronous preparation failure. Settings changes reapply active gains without changing fade progress. The music controller stores a pending key and looks up resources again at actual start. Missing resources or backend start failure log diagnostics and clear the target without automatic retries.

## Scene Sound Ownership

Every Scene owns `SceneAudioRuntime` by default, which stores resource keys, ordinary sound handles, logical loops and listener position. Gameplay uses AudioService uniformly: attachment borrows the runtime pointer and detach unbinds before user callbacks. Only one runtime can be attached at a time. The runtime holds no resource or game-object pointers; ResourceManager retains asset ownership. Scene refreshes distances after the game's `on_after_update`, before the application advances global audio scheduling.

Scene pause changes synchronize to owned sounds, freezing reservations and active fades separately. Scene pause and loop disable are independent reasons; resuming the scene removes only the former. The single request_sound defaults to scene ownership and FollowScene pause; SoundPlayOptions selects explicit Global scope or a spatial configuration. Global sounds ignore scene pause, and Global combined with spatial is rejected because listener ownership is scene-local. Instance setters recognize ownership and forward first; the runtime uses private service playback/gain methods to avoid recursive forwarding and refresh overwrites. Cleanup lives in nonvirtual lifecycle entrypoints, including reset and failure paths, so it does not rely on users calling base hooks.

Logical loops are separate from actual tracks: the silent band retains paused tracks, while exceeding release distance stops and returns a channel; audible return acquires a new voice. Failed acquisition retries use the runtime clock at most once every 250 ms by default without bypassing scheduler budgets, cooldown or replacement rules. Creation confirms the logical source; acquisition failure is observable through status and last_error. The error clears after acquiring a track or no longer needing playback. Missing-resource episodes suppress repeated diagnostics.

SoundHandle and LoopSoundId use separate process-wide monotonic identifiers that are not reused on reset; foreign-scene and previous-service-lifecycle handles cannot hit new sounds. The backend treats paused tracks as occupied and applies instance gain before startup. New regressions are in `sound_instance_control_tests`, `scene_audio_runtime_tests` and `audio_service_scene_tests`; dummy-device automation does not establish listening verification.

## Error Propagation and State Commit

AudioFailure pairs a specific AudioError with the shared FailureDiagnostic. Scheduler start, stop, gain, pause, and music callbacks preserve expected errors instead of reducing them to bool. SDL error text is captured before cleanup can overwrite it. Quiet has_sound and has_music checks avoid duplicate resource logging. Synchronous failures return to callers; validated parameter changes and fade progress commit after successful backend submission. Cleanup attempts all owned sounds and aggregates diagnostics.

A Scheduled result confirms queue acceptance. Failure at due time adds resource key and handle to its diagnostic, logs once with the named error code, and removes that reservation. No full playback history is kept. Internal source acquisition uses a private playback path, bypassing public routing without bypassing budgets. Runtime setters use private gain/pause paths to preserve ownership and avoid recursive routing.

SceneAudioConfig is always present; only retry tuning remains configurable. The service facade borrows exactly one attached runtime without exposing direct runtime access to gameplay. scene_audio_available reflects attachment, independently of device readiness. Built-in UI feedback explicitly selects Global so it can play before any scene attaches.

## Source and Tests

- [audio_service.cpp](../../../../engine/audio/audio_service.cpp): validation, track operations, scheduler callbacks, and floating-point gain.
- [mixer_backend.h](../../../../engine/audio/mixer_backend.h): shared device/track creation, reuse, and destruction, also used by built-in startup audio.
- [sound_playback_scheduler.cpp](../../../../engine/audio/sound_playback_scheduler.cpp): reservations, active instances, concurrency, cooldowns, and handles.
- [music_playback_controller.cpp](../../../../engine/audio/music_playback_controller.cpp): single-stream transition and stopping state machine.
- [sound_playback_scheduler_tests.cpp](../../../../tests/audio/sound_playback_scheduler_tests.cpp) and [audio_fade_tests.cpp](../../../../tests/audio/audio_fade_tests.cpp): scheduling/fade regressions. Automated passes do not constitute real-device listening acceptance.

- [audio_failure_contract_tests.cpp](../../../../tests/audio/audio_failure_contract_tests.cpp): injected backend errors, transactional state, deferred diagnostics, and global UI feedback without a scene.
