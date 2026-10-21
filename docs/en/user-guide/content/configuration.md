# Configuration and User Settings

This document distinguishes startup defaults, player settings, and read-only `game` configuration. Choose by data purpose and handle loading/application/persistence results; UI drafts are not effective configuration.

| Configuration | Provider and timing |
| --- | --- |
| `AppConfig` | Project startup defaults, edited in JSON before launch |
| `UserConfig` | Player settings applied/saved through `ELYSIA_USER_CONFIG` |
| Game configuration | Read-only through `ELYSIA_CONFIG` after content loading |

The application initializes user settings/runtime callbacks; loading publishes `game` configuration. Game objects only read/apply/save, without initializing, publishing snapshots, or shutting services down. See [resources](resources.md) for loading/files.

## Reading Game Configuration

Add `"game": "configs/game/rules.json"` under `configs` in `assets/configs/manifests/config_manifest.json`, and create that file, for example `{"difficulty":{"enemy_health_scale":1.0}}`. Call this after loading:

```cpp
#include "engine/config/config_service.h"
#include "engine/tools/logger.h"

#include <optional>
#include <expected>

std::optional<double> enemy_health_scale()
{
    const std::expected<double, elysia::config::ConfigAccessFailure> result =
        ELYSIA_CONFIG->get_double("game.difficulty.enemy_health_scale");
    if (!result)
    {
        ELYSIA_LOG_ERROR("config", result.error().message);
        return std::nullopt;
    }
    // Configuration checks types; positive health scale is a game rule.
    if (*result <= 0.0)
        return std::nullopt; // Game-specific rule.
    return *result;
}
```

On empty results, stop dependent spawning or use explicitly specified project defaults; the service has no automatic fallback. `contains(key)` means existence, not getter type compatibility.

Public getters are `get_int`, `get_double`, `get_bool`, `get_string`, `get_vector2`, `get_rect`, and their `_array` forms. Failures include `NotInitialized`, `MissingKey`, `TypeMismatch`, `InvalidValue`. Returned values own their data, without borrowing JSON.

`get_int` requires int64-range integers; `get_double` accepts finite numbers including integers. No implicit boolean/string conversion. `Vector2` requires exactly x/y; `Rect` exactly x/y/width/height, nonnegative size, finite float-representable components. Array getters apply the same rules to every element, without mixed types or `null`.

## Changing and Saving Player Settings

Call this helper from settings Save. For draft editing, retain entry `runtime_state()` as rollback baseline, edit an independent `snapshot()` copy, and save through the baseline overload.

```cpp
#include "engine/config/user_config_service.h"
#include "engine/tools/logger.h"

bool save_settings(const elysia::config::UserConfigData& draft,
                   const elysia::config::UserConfigRuntimeState& baseline)
{
    // baseline is the runtime state saved on editor entry, used for attempted rollback.
    const std::expected<elysia::config::UserConfigApplyStatus,
        elysia::config::UserConfigCommitFailure> result =
        ELYSIA_USER_CONFIG->apply_and_save_user_config(draft, baseline);
    if (!result)
    {
        const elysia::core::FailureDiagnostic diagnostic =
            elysia::config::to_failure_diagnostic(result.error());
        elysia::tools::Logger::instance()->log_stream(
            elysia::tools::LogLevel::Error, "settings",
            [&](std::ostream& output)
            {
                output << elysia::core::format_failure_diagnostic(
                    diagnostic, "SETTINGS", "settings");
            }, diagnostic.origin);
        return false; // Reread actual state; rollback is not guaranteed.
    }
    if (*result == elysia::config::UserConfigApplyStatus::PendingRestart)
        ELYSIA_LOG_INFO("settings", "Restart required for saved settings.");
    return true;
}
```

Apply individual values with `user_config().set_master_volume`, set_music_volume, set_sound_volume, set_language, set_target_fps, set_window_settings, or set_vsync. Check `expected` before reporting success. Setters do not save disk; `save_user_config()` persists current settings. Do not claim persistence on failure.

`snapshot()` copies settings; `runtime_state()` also includes restart state; `is_dirty()` reports unsaved changes and `restart_required()` pending restart. The baseline-free batch overload uses call-time state for rollback. See [settings scene](../builtin-scenes/settings.md) for complete page flow.
## AppConfig and UserConfig

`assets/configs/global/app_config.json` uses strict version 2 for title and window/render/audio/localization defaults. Logical resolution belongs only to `ApplicationDescriptor`, not user settings:

```json
{
  "schema_version": 2,
  "window": {
    "title": "Elysia Engine",
    "mode": "windowed",
    "windowed_size": { "width": 1280, "height": 720 }
  },
  "render": { "fps": 60, "vsync": true },
  "audio": { "master_volume": 100, "music_volume": 100, "sound_volume": 100 },
  "localization": { "language": "en" }
}
```

Unknown, missing, duplicate, or invalid fields fail startup. Window size/FPS must be positive, volumes `0..100`, title/language nonempty. `render.fps` caps presentation rate. VSync waiting and current update/render time share the frame budget; software limiting waits only the remainder.

`player_data/user_config.json` stores complete window/render/audio/localization snapshots without title. Both configurations accept strict v2 only. Old v0/v1 `UserConfig` is archived as invalid and rebuilt from `AppConfig`. `.tmp`/`.bak` recovery, corrupt-main archival, and future-version protection remain.

Window modes accept only `windowed` and `borderless_fullscreen`. `windowed_size` always retains `windowed` dimensions; entering desktop borderless fullscreen does not overwrite it.

Built-in `SettingsScene` uses draft submission: controls do not immediately change runtime. Save batches application/persistence through `UserConfigService::apply_and_save_user_config()`, explicitly carrying entry/last-successful-save `UserConfigRuntimeState` as rollback baseline. Application/persistence failures therefore do not roll back to incidental pre-click state. Rollback failures return independently; the page refreshes from actual `UserConfig` runtime state.

FPS cap applies immediately to the software limiter. VSync saves for next startup and shows restart status; reverting to the startup-applied value cancels it. The built-in panel groups display, audio, and general settings in one scroll region, with title/status/Save/Back fixed. See [settings scene](../builtin-scenes/settings.md).

## Generic Gameplay Configuration

`manifests.required.configs` points to `assets/configs/manifests/config_manifest.json`:

```json
{
  "schema_version": 1,
  "configs": {}
}
```

`configs` maps namespaces to document paths. The example is empty; add character/skill/level namespaces and paths for centralized content loading. The manifest itself is strict. Referenced document roots may be object, array, string, boolean, or number, but never `null`.

Root values register under namespace keys; object fields expand recursively; array indices use unpadded `.0/.1/...` components. For example:

```json
{
  "difficulty": { "enemy_health_scale": 1.0 },
  "spawn_points": [{ "x": 100, "y": 200 }]
}
```

With namespace `game`, query `game.difficulty.enemy_health_scale` or `game.spawn_points.0.x`. Namespace/field components must match `[A-Za-z0-9_]+`. Duplicate properties, `null` anywhere, invalid components, and full-key conflicts fail before snapshot publication, retaining configuration path, JSON pointer, and first/duplicate declaration sources.

Game code reads through `ConfigService` `contains`, scalar/geometry, and array getters. Loading owns `publish` and shutdown. Access failures return `expected` and deduplicate logs by error type. No raw JSON, optional, implicit fallback, hot reload, or business-specific API is exposed.

## Common Misuse and References

Immediate audio volume/localization changes do not persist preferences; use user configuration. Read-only configuration is not progress storage; use [saves](save.md). Disk edits do not hot reload. Reloading content can invalidate resources; see [resource lifetime](resources.md).

- [Configuration API](../../../../engine/config/config_service.h), [user settings API](../../../../engine/config/user_config_service.h)
- [Optional architecture](../../architecture/subsystems/runtime-config.md), [back to user guide](../README.md)
