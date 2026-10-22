# Startup Loading Scene

This guide configures startup loading routes, project logos, and confirmation, and explains loading failures. Presentation completion and resource readiness are independent conditions; choose waiting policies for your project.

`StartupLoading` handles loading presentation, project logos, and confirmation before game content entry. The application still assembles startup and prepares built-in resources; this does not replace `IGameModule`. See [resource loading](../content/resources.md) for manifests.

## Configuring the Initial Route

Place this function in the module implementation and use its result for `ApplicationDescriptor::initial_route`. Pass a valid route to a registered game scene, such as the main menu, not an arbitrary unregistered number.

```cpp
#include "engine/builtin/builtin_scene_keys.h"
#include "engine/builtin/scenes/startup_loading_scene_payload.h"

#include <utility>

elysia::scene::SceneRoute make_startup_route(
    elysia::scene::SceneRoute main_menu)
{
    elysia::builtin::StartupLoadingScenePayload payload;
    // After loading, enter the caller's main menu; routes can carry its business Payload.
    payload.success_route = std::move(main_menu);
    // This only demonstrates loading, without logo waits or extra confirmation.
    payload.wait_for_logo_sequence = false;
    payload.wait_for_confirmation = false;
    return {
        .target = elysia::builtin::SceneKeys::StartupLoading,
        .payload = std::move(payload),
        .reload_mode = elysia::scene::SceneReloadMode::Reuse
    };
}
```

Entry requires `StartupLoadingScenePayload` with a valid `success_route`; wrong types or invalid routes throw integration errors. The target must also be registered; a valid route value does not establish existence.

## Payload Fields

| Field | Usage |
| --- | --- |
| `success_route` | Target and Payload after loading |
| `failure_route` | Optional custom failure route; absent means built-in failure display |
| `project_logo` | Optional `StartupLogoSlot` with texture resource key and fade-in, hold, fade-out seconds |
| `wait_for_logo_sequence` | Default `true`; `false` stops waiting for logo completion once loading finishes |
| `wait_for_confirmation` | Default `true`; successful loading still requires user confirmation, not immediate switching |

Project logos use preloaded texture keys, not file paths. The scene queries `Bootstrapper`; placing the logo only in later content manifests is insufficient. Empty keys or unavailable textures log warnings and skip the optional logo.

By default, logo presentation and loading must both finish before confirmation/success switching. With `wait_for_logo_sequence=false`, successful loading advances immediately: if `wait_for_confirmation=true`, show confirmation while logos can continue in the background; if both are `false`, switch immediately to `success_route`. Early switching bypasses neither failures nor project font activation.

## Failure and Reentry

Loading failure uses supplied `failure_route` first; otherwise it constructs built-in `ApplicationFailure` with diagnostics. Custom failure scenes must handle their route Payload themselves; arbitrary custom Payload does not automatically acquire error fields.

Every entry restarts project loading and first clears old content. This is not a hot-reload transaction preserving old resources: old resource pointers cannot be used after failure. Cached scenes retaining textures, atlases, or fonts must detach those references first. Restarting after resource edits more easily preserves complete dependencies.

Project preload textures release on exit; reentry does not automatically repeat application preload. Do not rely on the same logo appearing again. `content_registry.json` remains the startup snapshot; registry edits require restart. See [application failure scene](application-failure.md) for error-display boundaries.

## Loading Failure Diagnostics

To locate configuration, platform, or resource problems from a startup message, see [startup failures and troubleshooting](../game-initialization.md#7-startup-failures-and-troubleshooting). It also explains how to preserve diagnostics before file logging is available.

Preparation-thread failures return to the main thread, stop further loading, clean incomplete resources, then follow failure routing. Games need no thread management; retain `ContentLoadFailure` stage/resource diagnostics to identify files, manifests, or loading steps. Loader reset capability does not automatically restore game objects depending on old resources.

## References

- [Payload definition](../../../../engine/builtin/scenes/startup_loading_scene_payload.h)
- [Repository game module](../../../../game/application/example_game_module.cpp)
- [Loader implementation](../../../../engine/loading/game_content_loader.cpp)
- [Game initialization](../game-initialization.md), [back to user guide](../README.md)
