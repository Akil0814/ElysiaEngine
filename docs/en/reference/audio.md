# Audio Interfaces and Playback Contracts

This page maintains `AudioService` parameters, results, handles, concurrency, and volume contracts. See [audio guide](../user-guide/systems/audio.md) for integration/examples and [architecture](../architecture/subsystems/audio.md) for scheduling.

Definitions: [audio_service.h](../../../engine/audio/audio_service.h), [sound_playback_types.h](../../../engine/audio/sound_playback_types.h), [audio_settings.h](../../../engine/audio/audio_settings.h), [music_playback_controller.h](../../../engine/audio/music_playback_controller.h).

## Sound Playback and SoundHandle

Signatures/structures excerpt relevant members, omitting namespaces, some defaults, and declarations. Use them to look up parameters, not to define duplicate project types. Refer to the headers above.

### `play_sound`

```cpp
bool play_sound(const std::string_view& key, int loops = 0);
```

Simplified immediate playback defaults to `SoundGroup::Extra` and returns only whether playback actually started.

- loops=0: play once.
- loops>0: additional repeats, total loops+1.
- loops=-1: SDL_mixer infinite looping.

No `handle` is returned, so this is unsuitable for precisely stopping sustained sounds later.

### `request_sound`

```cpp
SoundRequestResult request_sound(
    const std::string_view& key,
    const SoundPlayOptions& options = {});
```

Recommended entry: validate the key, then immediately play or schedule according to `options`.

```cpp
struct SoundPlayOptions
{
    std::optional<int> loops;                 // nullopt means one playback
    SoundGroup group = SoundGroup::Extra;
    std::chrono::milliseconds start_delay{0};
    std::chrono::milliseconds fade_in{0};
};
```

Result:

```cpp
enum class SoundRequestStatus
{
    Started,
    Scheduled,
    Rejected
};

struct SoundRequestResult
{
    SoundRequestStatus status;
    std::optional<SoundHandle> handle;
};
```

- `Started`: logical channel started; `handle` refers to active playback.
- `Scheduled`: queued; `handle` refers to pending playback, still subject to `cooldown`/concurrency at due time.
- `Rejected`: uninitialized, missing resource, immediate `cooldown`/concurrency, or backend failure; no `handle`.

Due requests failing concurrency/`cooldown`/backend are discarded without queuing/retry.

### `stop_sound`

```cpp
bool stop_sound(SoundHandle handle, std::chrono::milliseconds fade_out = {});
```

Unified stop/cancel:

- Pending `handle`: cancel and return `true`.
- Playing `handle`: accept stop, return `true`. Positive duration fades current gain to zero, then stops/removes. Handle remains valid and occupies concurrency during fade.
- Stopping during fade-in fades from current gain. Repeated fade-out does not restart timing; nonpositive durations stop immediately. Natural completion cleans earlier.
- Ended/replaced/invalid handles or uninitialized service return `false`.

### `cancel_all_scheduled_sounds`

```cpp
void cancel_all_scheduled_sounds();
```

Clears pending requests only, not active sounds. Conversely:

```cpp
void stop_all_sounds(std::chrono::milliseconds fade_out = {});
```

Stops/fades active sounds only, not pending ones. Durations are milliseconds, default zero; nonpositive completes immediately.

## Concurrency Groups, Cooldown, and Overflow

Every request belongs to fixed `SoundGroup`:

| Group | Hard limit | Typical use |
| --- | ---: | --- |
| `Ui` | 4 | Focus, clicks, selections, menu feedback |
| `Gameplay` | 12 | Actors, skills, hits, footsteps |
| `Ambient` | 4 | Sustained environment/scene sound |
| `Extra` | 4 | Unclassified/general sound |

Groups share 24 sound slots, excluding music. Limits sum to 24; idle other groups cannot increase one group's hard limit.

Configuration types:

```cpp
struct SoundGroupConfig
{
    std::optional<std::size_t> max_simultaneous;
    std::chrono::milliseconds cooldown{0};
    SoundOverflowPolicy overflow_policy = SoundOverflowPolicy::IgnoreNew;
};
```

```cpp
enum class SoundOverflowPolicy
{
    IgnoreNew,
    ReplaceOldest,
};
```

### Rule Semantics

- `max_simultaneous`=nullopt uses the hard limit.
- Explicit limits may only lower it; 0 is allowed.
- Cooldown records last successful playback by **resource key**, using the requesting group's duration. Different keys are independent; the same key shares history across groups. Only successful starts refresh it.
- `IgnoreNew` rejects at capacity, default for UI/ambience.
- `ReplaceOldest` stops the group's earliest active sound before starting new; replaced handles invalidate. Other groups are untouched.
- If a group has space but all 24 channels are full, reject. No cross-group priorities/preemption.

Lowering limits does not stop active sounds. `IgnoreNew` rejects when at limit; `ReplaceOldest` replaces only one at a time and does not immediately converge if already over limit. Zero is not a disable mechanism: empty groups reject, but replacement with active sounds may still start new ones.

Public configuration API:

```cpp
bool set_sound_group_config(SoundGroup group, const SoundGroupConfig& config);
const SoundGroupConfig& sound_group_config(SoundGroup group) const;
```

Limits above hard maximum or negative `cooldown` make set_sound_group_config return `false`, preserving old configuration.

## Volume Model

### Global and Music Volume

`AudioSettings` contains:

```cpp
struct AudioSettings
{
    int master_volume = 100;
    int music_volume = 100;
    int sound_volume = 100;
};
```

Modify at runtime:

```cpp
void set_master_volume(int volume);
void set_music_volume(int volume);
void set_sound_volume(int volume);
const AudioSettings& settings() const;
```

Writes clamp to `0..100`.

Effective music volume:

```text
music_gain = master_volume / 100 × music_volume / 100 × music_fade_gain
```

### Group Volume

Each group has nonpersistent runtime volume:

```cpp
void set_sound_group_volume(SoundGroup group, int volume);
int sound_group_volume(SoundGroup group) const;
```

- Inputs clamp to `0..100`.
- New channels immediately use effective group volume.
- Changing master/sound/group volume immediately updates active channels in the relevant groups.
- Group volume is absent from `AudioSettings`/user settings/manifests; initialization restores all to `100`.

Effective sound gain:

```text
sound_gain = master_volume / 100 × sound_volume / 100 × group_volume / 100 × instance_fade_gain
```

Division uses floating point and `MIX_SetTrackGain`, without integer maximum multiplication/truncation. Fade gain is independent linear `0..1`; volume changes apply immediately without modifying fade settings. Initial gain is set before playback to avoid full-volume first frames.

`music_volume` does not affect sounds; `group_volume` does not affect music.

## Music API

```cpp
bool play_music(const std::string_view& key, int loops = -1,
    std::chrono::milliseconds fade_in = {});
void stop_music(std::chrono::milliseconds fade_out = {});

struct MusicTransitionOptions
{
    int loops = -1;
    std::chrono::milliseconds fade_out{0};
    std::chrono::milliseconds fade_in{0};
};
bool transition_music(const std::string_view& key,
    const MusicTransitionOptions& options = {});
```

- play_music validates then immediately replaces playback/clears pending target/fades new music in. Same keys restart.
- transition_music fades old out → starts new → fades new in. Without music it starts directly; natural old completion starts target next update.
- One pending target. New requests during fade-out replace key/loops/fade-in while preserving fade-out progress/end.
- Requesting the currently playing key cancels pending target, keeps position/loops, and restores gain to 1 using requested fade-in. At full gain it continues.
- stop_music clears pending target and fades current gain. Repeated fades do not restart; nonpositive stops immediately.
- Keys must be loaded. Uninitialized/invalid requests return `false` without changing playback/accepted transition. Default loops=-1 repeats continuously.
- transition_music `true` means acceptance, not guaranteed startup. It retains keys and re-queries at startup. Missing resources/SDL failure log, clear target, enter idle without retry. Synchronous start failure returns `false`.
- Music bypasses sound groups/`cooldown`/delayed requests/handles/24 slots. This is one sequential channel, not simultaneous crossfade.

## Lifecycle and Borrowed Results

`shutdown` stops immediately and clears delayed/transition/fade state without waiting. Reinitialization cleans previous state first.

Application drives `initialize`/update/`shutdown`; games call playback after initialization/content success. is_initialized queries readiness. `SoundHandle` is std::uint64_t without owning sounds/resources/scenes; discard after `shutdown`/reinitialization.

settings/sound_group_config return read-only internal references; copy snapshots if retained. Setters change runtime; [user configuration](../user-guide/content/configuration.md) persists.

## Current Boundaries

The module **does not provide**:

- Waiting at capacity, automatic retries, sequential queues.
- Cross-group priority/preemption.
- Dynamic custom groups.
- Per-instance pause/resume, panning, pitch, random variants.
- Multi-track crossfade, music queues, group-volume fades, or built-in startup-player fade extensions.
- Group-volume persistence.
- Automatic scene/entity/skill batch cancellation of delayed requests.

Design these separately when concrete gameplay/product needs arise, avoiding renewed coupling among loading, scene lifetime, and playback scheduling.
