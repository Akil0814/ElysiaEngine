# Settings Scene

This document explains opening built-in settings and separating drafts, actual runtime state, and disk settings. Return routes determine game scene restoration; save results determine which settings take effect.

Built-in `Settings` provides window, frame-rate, VSync, volume, and `language` settings. `UserConfigService` must be initialized and the scene must receive valid `SettingsScenePayload`. This is a separate scene, not a pause panel automatically overlaid on gameplay.

## Opening from a Game Scene

This empty scene demonstrates opening settings. Put `open_settings()` in your derived scene and call it from a button callback or normal input/update. `return_route` targets a registered scene with its own Payload as needed.

```cpp
#include "engine/builtin/builtin_scene_keys.h"
#include "engine/builtin/scenes/settings_scene_payload.h"
#include "engine/scene/scene.h"

#include <utility>

class SettingsExampleScene final : public elysia::scene::Scene
{
public:
    void open_settings(elysia::scene::SceneRoute return_route)
    {
        elysia::builtin::SettingsScenePayload payload;
        // Move return route and Payload together, preserving caller restoration mode.
        payload.return_route = std::move(return_route);
        payload.visibility.target_fps = false;
        // This only requests routing; the manager switches at a safe phase.
        request_scene_switch(elysia::scene::SceneRoute{
            .target = elysia::builtin::SceneKeys::Settings,
            .payload = std::move(payload),
            .reload_mode = elysia::scene::SceneReloadMode::Reuse
        });
    }

protected:
    // This empty scene only opens settings, with no business objects to initialize/clean.
    void on_enter(const elysia::scene::ScenePayload&) override
    {
    }

    void on_exit() override
    {
    }

    void on_reset() override
    {
    }
};
```

`request_scene_switch` is protected and cannot be called from ordinary free functions. Do not request switching during `on_enter()`, `on_exit()`, `on_reset()`, or rendering. Missing or incorrectly typed Payload, invalid return routes, or uninitialized configuration service cause integration errors.

## Visible Items and Saving

All `visibility` fields—`window_mode`, `target_fps`, `vsync`, `master_volume`, `music_volume`, `sound_volume`, `language`—default to `true`. They control available UI items, not new application capabilities or configuration defaults.

The panel maintains a draft; changing controls does not mean saving. Save assembles a request from the current configuration snapshot and calls `apply_and_save_user_config` to apply/persist it.

Display, audio, and general settings share one scroll region; title, status, and Save/Back remain fixed. Scroll down for content exceeding available height; no tab switching is needed.

- Success: refresh runtime baseline and draft, display saved/restart-required status; do not automatically return to gameplay.
- Failure: display a short localized failure and log full details. The service attempts rollback; refresh the draft from the actual post-operation runtime snapshot, without assuming all requested or all old values remain.
- Back/cancel: discard unsaved draft and leave through `return_route`; previously saved settings are not undone.

Languages come from the localization service's supported list; window sizes consider available display area. Hiding an item does not remove its setting, and not every setting applies immediately/unconditionally.

FPS cap, volume, and `language` can apply at runtime; VSync needs restart after saving. Reverting VSync to the startup value cancels restart-pending state. See [configuration and user settings](../content/configuration.md) for file format and individual changes.

## Return and Lifecycle

Return route Reuse/Reset/Recreate determines target restoration. Avoid Recreate when retaining level state. Exit detaches panel/window associations and hides/deactivates UI; cached instances can be reused. Game input bindings and reentry remain game responsibilities.

## Handling Save Failure

Window changes, runtime application, and disk `persistence` can all fail. The engine attempts restoration, which can also fail; a failed call does not make the draft actual state. The built-in page rereads state; custom pages should do likewise.

`UserConfigCommitFailure::cause` retains original failure; `rollback_failure` describes runtime rollback failure; optional `persistence` describes disk restoration. Successful disk restoration does not prove window/volume restoration. For custom logs, aggregate via `to_failure_diagnostic()`; see [configuration guide](../content/configuration.md#changing-and-saving-player-settings).

File access failures are not treated as corruption and overwritten with defaults. Temporary copies that cannot be safely restored are retained. Report failure, preserve diagnostics, then decide whether to retry. Unexpected exceptions can still propagate to application boundaries; `expected` does not swallow all failures.

## References

- [Payload](../../../../engine/builtin/scenes/settings_scene_payload.h), [visibility and drafts](../../../../engine/ui/presets/settings_panel.h)
- [Runtime configuration](../content/configuration.md), [localization](../systems/localization.md)
- [Commit/rollback implementation](../../../../engine/config/user_config_service.cpp), [back to user guide](../README.md)
