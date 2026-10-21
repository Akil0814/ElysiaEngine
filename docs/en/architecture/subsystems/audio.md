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
- `AudioService` is the public gameplay/UI playback entry point. It neither depends on `Scene` nor owns scene objects.
- Internal `SoundPlaybackScheduler` manages pending/active sounds, `SoundHandle`s, concurrency groups, cooldowns, and overflow policy.
- SDL3_mixer plays tracks. Scheduler channels are engine logical indices; resource lookup and track operations are adapted by the service/backend, not SDL2_mixer channel APIs.

## Lifecycle and Frame Update

The application calls `MIX_Init`, then checks the boolean result of `AudioService::initialize`, stopping dependent initialization on failure. Through shared `MixerBackend`, the service creates or reuses the device, sound tracks, and independent music track, resets scheduling/group volumes, and applies initial volume. Reinitialization first clears the previous lifecycle's state.

Each frame, after scene update and switching, the application calls `AudioService::update` with scaled delta. The service advances `SoundPlaybackScheduler` before `MusicPlaybackController`; negative or non-finite delta becomes zero. Due reservations and transition targets start after existing fades are processed; their fade-in does not consume that update's delta again. The device independently plays started tracks, so scheduling-clock changes do not change track playback speed automatically.

Shutdown stops all tracks and clears reservations, active instances, music targets, and fades without waiting for fade-out. The application then clears resources and shuts down the shared backend to destroy tracks and device. The service does not know scene, entity, or skill lifetimes.

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

Set initial gain before starting a track to avoid a full-volume first frame. Settings changes reapply active gains without changing fade progress. The music controller stores a pending key and looks up resources again at actual start. Missing resources or backend start failure log diagnostics and clear the target without automatic retries.

## Source and Tests

- [audio_service.cpp](../../../../engine/audio/audio_service.cpp): validation, track operations, scheduler callbacks, and floating-point gain.
- [mixer_backend.h](../../../../engine/audio/mixer_backend.h): shared device/track creation, reuse, and destruction, also used by built-in startup audio.
- [sound_playback_scheduler.cpp](../../../../engine/audio/sound_playback_scheduler.cpp): reservations, active instances, concurrency, cooldowns, and handles.
- [music_playback_controller.cpp](../../../../engine/audio/music_playback_controller.cpp): single-stream transition and stopping state machine.
- [sound_playback_scheduler_tests.cpp](../../../../tests/audio/sound_playback_scheduler_tests.cpp) and [audio_fade_tests.cpp](../../../../tests/audio/audio_fade_tests.cpp): scheduling/fade regressions. Automated passes do not constitute real-device listening acceptance.
