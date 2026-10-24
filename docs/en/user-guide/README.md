# Elysia Engine User Guide

This guide helps developers use Elysia Engine to build games, from environment setup and running builds to developing game content.

## Getting Started

Start with [building, running, and testing](build-and-run.md) for toolchain requirements, build commands, working directories, tests, and troubleshooting.

## Read Before Developing

Before developing game content, read:

- [Core concepts](core_concepts.md): responsibilities of scenes, game objects, and UI; object lifetime, updates, and destruction timing.

## Building a Game from Scratch

To create a game from scratch, read [game initialization and integration](game-initialization.md) for `IGameModule` responsibilities, entry point organization, and registering/initializing game content.

If you are adding actors, weapons, UI, or other features to an existing game, skip this section and use the task-specific documentation.

## Gameplay Features

Elysia Engine provides foundational gameplay modules for scenes, actor control, and gameplay collision logic.

| Module | Purpose | Guide |
| --- | --- | --- |
| Gameplay Collision | Gameplay collisions, collision events, and relationships | [Gameplay collision](gameplay/collision.md) |
| Gameplay Control | Controller management, control commands, and scene control contexts | [Controllers and control commands](gameplay/control.md) |
| `GameplayScene` | Levels requiring actor control or gameplay collisions | [GameplayScene guide](gameplay/scene.md) |

For a complete combat flow, read [from attack hits to receiving damage](gameplay/damage.md). This is a game-layer composition example: the engine supplies hit events; the game implements health, damage calculation, and death rules.

C++ examples follow the [coding standard](../contributing/coding-standard.md): private members use `_name_name`, named constants use `kPascalCase`, and ordinary variables favor explicit types. Local fragments state their phase and prerequisites. See the [build guide](build-and-run.md#running-tests) for compilation checks of complete examples.

## Browse by Development Task

The following documents are organized by development task. Choose as needed.

| Task | Entry point |
| --- | --- |
| Create scenes, manage objects, pause, and switch | [Scene system](scene/scene.md) |
| Write object behavior, choose drawing layers, submit drawing commands | [Game objects and rendering](scene/game-objects.md) |
| Register content, configure preloading and manifests | [Resource loading and use](content/resources.md) |
| Configure input devices, action maps, and scene input | [Input system](systems/input.md) |
| Create UI windows, organize layout, compose widgets | [UI guide](ui/README.md#create-window) |
| Display world nameplates, health bars, and bubbles | [World-space UI](gameplay/world-ui.md) |
| Integrate map collision, refresh cells, drop through platforms | [Tile map collision](systems/tilemap-collision.md) |
| Enable development overlays and register panels | [Development panels](tools/development-panels.md) |
| Look up public widget interfaces | [UI API reference](../reference/ui/README.md) |
| Use rigid bodies, physics collision events, spatial queries, and TileMap collision | [Physics integration](systems/physics.md) |
| Implement attacks, damage reception, and death | [From attack hits to receiving damage](gameplay/damage.md) |
| Configure animation resources, atlases, and effects | [Animation and effects](systems/animation-and-effects.md) |
| Play music/sounds, control volume and scheduling | [Audio service](systems/audio.md) |
| Use camera following, shake, and coordinate conversion | [Camera workflow](systems/camera.md) |
| Manage startup configuration, user settings, and game configuration | [Runtime configuration](content/configuration.md) |
| Read/write saves, handle recovery and errors | [Save service](content/save.md) |
| Use time scaling, fixed-step updates, and timer callbacks | [Time and timers](systems/time-and-timers.md) |
| Query translations, switch languages, generate text textures | [Localization and text](systems/localization.md) |
| Find game objects by type, drawing layer, and spatial conditions | [Game object queries](systems/object-query.md) |
| Draw debug primitives and manage categories | [Debug drawing](tools/debug-draw.md) |
| Use vectors, rectangles, `colors`, random numbers, and logging | [Common utilities](tools/utilities.md) |

## Common Macros and Service Entry Points

Elysia Engine provides convenience macros for several common services.

The following table lists their purpose, related documentation, and interface headers. Refer to those documents or headers for usage.

| Entry point | Purpose | Guide | Interface |
| --- | --- | --- | --- |
| `ELYSIA_ANIMATIONS` | Animation service | [Animation and effects](systems/animation-and-effects.md) | [animation_service.h](../../../engine/animation/animation_service.h) |
| `ELYSIA_AUDIO` | Audio service | [Audio service](systems/audio.md) | [audio_service.h](../../../engine/audio/audio_service.h) |
| `ELYSIA_CONFIG` | Typed game configuration reads | [Runtime configuration](content/configuration.md) | [config_service.h](../../../engine/config/config_service.h) |
| `ELYSIA_USER_CONFIG` | User settings management | [Runtime configuration](content/configuration.md) | [user_config_service.h](../../../engine/config/user_config_service.h) |
| `ELYSIA_TIME` | Time facilities | [Time and timers](systems/time-and-timers.md) | [time.h](../../../engine/core/time.h) |
| `ELYSIA_EFFECTS` | Effect service | [Animation and effects](systems/animation-and-effects.md) | [effect_service.h](../../../engine/effects/effect_service.h) |
| `ELYSIA_LOCALIZATION` | Localization service | [Localization and text](systems/localization.md) | [localization_service.h](../../../engine/localization/localization_service.h) |
| `ELYSIA_OBJECT_QUERY` | Game object queries | [Game object queries](systems/object-query.md) | [game_object_query_service.h](../../../engine/object_query/game_object_query_service.h) |
| `ELYSIA_RESOURCES` | Loaded resource queries | [Resource loading and use](content/resources.md) | [resource_service.h](../../../engine/resources/resource_service.h) |
| `ELYSIA_SAVE` | Save management | [Save service](content/save.md) | [save_service.h](../../../engine/save/save_service.h) |
| `ELYSIA_DEBUG_DRAW` | Debug drawing | [Debug drawing](tools/debug-draw.md) | [debug_draw.h](../../../engine/tools/debug_draw.h) |
| `ELYSIA_CONTROLLERS` | Control sessions, controllers, and target binding | [Controllers and control commands](gameplay/control.md) | [controller_service.h](../../../engine/gameplay/control/controller_service.h) |
| `elysia::gameplay::collision::GameplayCollisionService::instance()` | Gameplay collision binding, event listeners, attack instance cleanup | [Gameplay collision](gameplay/collision.md) | [gameplay_collision_service.h](../../../engine/gameplay/collision/gameplay_collision_service.h) |

### Logging Macros

The engine provides log macros at different levels for runtime information, debugging, warnings, and errors.

| Macro | Level | Purpose |
| --- | --- | --- |
| `ELYSIA_LOG_DEBUG` | Debug | Debug information |
| `ELYSIA_LOG_INFO` | Info | Ordinary runtime information |
| `ELYSIA_LOG` | Info | Convenience entry point for Info |
| `ELYSIA_LOG_WARN` | Warn | Warnings |
| `ELYSIA_LOG_ERROR` | Error | Errors |
| `ELYSIA_LOG_TERMINATING` | Terminating | Termination-level log; does not itself exit the application |

See [common utilities](tools/utilities.md) for categories, streamed messages, and error handling conventions.

Basic usage:

```cpp
#include "engine/tools/logger.h"

// Log inside business functions:
ELYSIA_LOG_INFO("Gameplay", "Game initialized.");
ELYSIA_LOG_ERROR("Resources", "Failed to load texture.");
```

## Application Error Display and Exit

Elysia Engine supplies a built-in application failure scene for errors that cannot recover within the current scene while still allowing an error interface to run.

When gameplay cannot continue but the window, renderer, and required built-in resources remain available, request the failure scene to display diagnostics and let the user exit. It provides no general retry or continue-game feature; recoverable business failures belong to the game's own flow.

See [application failure scene](builtin-scenes/application-failure.md) for usage.

## Other General Utilities

Besides services, Elysia Engine provides reusable development utilities.

| Utility | Purpose | Interface |
| --- | --- | --- |
| `RandomGenerator` | Random number generation | [random_generator.h](../../../engine/tools/random_generator.h) |
| `Timer` | Timing | [timer.h](../../../engine/tools/timer.h) |
| `Vector2` | 2D vectors, directions, distances, normalization | [vector2.h](../../../engine/core/geometry/vector2.h) |
| `Rect` | Rectangle bounds, containment, intersection | [rect.h](../../../engine/core/geometry/rect.h) |
| `Color` / `colors` | RGBA values and predefined `colors` | [color.h](../../../engine/core/render/color.h), [colors.h](../../../engine/core/render/colors.h) |

See [common utilities](tools/utilities.md) for vector, rectangle, color, and random-number usage and boundaries; see [time and timers](systems/time-and-timers.md) for `Timer` driving and lifetime.

## Built-In Scenes

Elysia Engine provides these scenes for common application flows.

| Scene key | Purpose | Guide |
| --- | --- | --- |
| `StartupLoading` | Project content loading, logo display, completion confirmation | [Startup loading scene](builtin-scenes/startup-loading.md) |
| `Settings` | Game settings interface | [Settings scene](builtin-scenes/settings.md) |
| `ApplicationFailure` | Application error display and exit | [Application failure scene](builtin-scenes/application-failure.md) |

These keys belong to `elysia::builtin::SceneKeys`, defined in [builtin_scene_keys.h](../../../engine/builtin/builtin_scene_keys.h). The application registers these built-in scenes; games use them through routes.

For configuration, data transfer, and conventions, consult each guide, scene definition, and Payload header.

## Further Reading

- [Interface and format reference](../reference/README.md): complete API contracts and resource formats.

- [Architecture and module design](../architecture/README.md): internal responsibilities and runtime mechanisms.
- [Development and contribution guide](../contributing/README.md): coding, testing, and documentation maintenance.
- [Design documents and roadmap](../design/README.md): future plans, proposals, and pending architecture migrations.
- [Back to project introduction](../../../README.md): project purpose, features, and repository structure.
