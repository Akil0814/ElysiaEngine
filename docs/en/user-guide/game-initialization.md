# Game Project Initialization Guide

This guide explains the responsibilities and connections of the program entry point, game module, application configuration, and scene registration. You should already be able to build the repository using the [build and run guide](build-and-run.md).

The entry point hands execution to the engine. The game module supplies configuration and registers game scenes; the engine initializes subsystems and drives scenes. After explaining these roles, this guide gives minimal module and startup-loading route examples. They use the repository's CMake include paths; module code compiles, but execution still requires valid project configuration and resources.

If the project builds but reports a startup error, fails to load, or never reaches the target scene, go directly to [startup failures and troubleshooting](#7-startup-failures-and-troubleshooting).

## 1. Initialization Flow

The engine obtains the application descriptor and registers game scenes through `IGameModule`. `ELYSIA_INITIALIZE_APP` does more than save that descriptor: it prepares the runtime environment and starts the initial scene. Only after success does `ELYSIA_RUN_APP` continuously process input, updates, and drawing. Content loading separately publishes read-only game configuration from resources; see [configuration guide](content/configuration.md).

```text
main()
  |
  +-- Create GameModule
  |
  +-- ELYSIA_INITIALIZE_APP
  |     +-- Obtain and validate ApplicationDescriptor
  |     +-- Read runtime configuration; initialize window, renderer, and other subsystems
  |     +-- Prepare built-in resources, input, and scene runtime context
  |     +-- Register built-in scenes, then call register_scenes() for game scenes
  |     +-- Start the initial scene from initial_route
  |
  +-- Call ELYSIA_RUN_APP after successful initialization
  |     +-- Input, update, drawing
  |     +-- Clean up runtime resources on exit
  |
  +-- Return process exit status
```

This lists game integration stages, not every internal initialization step. Games need not initialize each subsystem individually in `main()`.

## 2. Program Entry Point

The following preserves the startup and outermost exception handling in [main.cpp](../../../main.cpp). It uses the existing example module as a reference when replacing the entry point.

```cpp
#include <SDL3/SDL_main.h>
#include "engine/application/application.h"
#include "game/application/example_game_module.h"

#include <cstdlib>

int main(int argc, char** argv)
{
    try
    {
        example::application::GameModule game_module;
        if (!ELYSIA_INITIALIZE_APP(argc, argv, game_module))
            return EXIT_FAILURE;

        return ELYSIA_RUN_APP ==
                       elysia::application::ApplicationRunResult::NormalExit
                   ? EXIT_SUCCESS
                   : EXIT_FAILURE;
    }
    catch (...)
    {
        return EXIT_FAILURE;
    }
}
```

Do not call `ELYSIA_RUN_APP` after initialization returns `false`. Return success for `NormalExit`, failure for other results.

The engine catches exceptions during initialization and major runtime stages and handles failure at the relevant boundaries. Entry-point `catch (...)` handles C++ exceptions still propagating here, such as game module construction failures. It only sets a failure exit code; it neither resumes gameplay nor adds logging here. An exception escaping a `noexcept` function invokes `std::terminate()`; this outer catch cannot keep cleanup running.

## 3. Defining the Game Module

A game module derives from `elysia::application::IGameModule` and supplies game-specific configuration and scenes. See [game_module.h](../../../engine/application/game_module.h).

### 3.1 IGameModule Interface

| Method | Responsibility | Required |
| --- | --- | --- |
| `descriptor() const` | Return `ApplicationDescriptor` with logical resolution, `presentation` settings, and initial route | Yes |
| `register_scenes(SceneManager&) const` | Register game scenes with the manager | Yes |
| `create_development_overlay() const` | Return owning `std::unique_ptr<IDevelopmentOverlay>` for the engine to acquire | No |

`descriptor()` supplies data; `register_scenes()` associates identifiers with scene types. Registration does not construct every game scene immediately; the manager creates instances when needed. The development factory defaults to null and needs no override without an overlay.

### 3.2 Creating Your GameModule

This complete example can live in a header such as `game/application/my_game_module.h`. It registers an empty scene as the initial target, showing that configuration and registration must use the same valid identifier.

```cpp
#pragma once

#include "engine/application/game_module.h"
#include "engine/scene/scene.h"
#include "engine/scene/scene_manager.h"

namespace initialization_example
{
// Example scene key; keep it unique among the project's game scenes.
inline constexpr elysia::scene::SceneKey kEntry = 1;

class EntryScene final : public elysia::scene::Scene
{
protected:
    // The minimal entry owns no business objects, so all three lifecycle hooks are empty.
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

class GameModule final : public elysia::application::IGameModule
{
public:
    [[nodiscard]] elysia::application::ApplicationDescriptor
        descriptor() const override
    {
        elysia::application::ApplicationDescriptor descriptor;
        descriptor.logical_width = 1280;
        descriptor.logical_height = 720;
        // After registration the application activates the entry through this route.
        descriptor.initial_route = {.target = kEntry};
        return descriptor;
    }

    void register_scenes(
        elysia::scene::SceneManager& scene_manager) const override
    {
        scene_manager.register_game_scene<EntryScene>(kEntry);
    }
};
}
```

Replace the entry point's module include and type with your header and `initialization_example::GameModule`. Repository `engine_lib` and `game_lib` include the project root, so `engine/...` paths do not depend on a header's directory depth.

`EntryScene` adds no visible content and loads no project content. Entering it directly is useful for checking module wiring. Scenes depending on project resources/fonts should use startup loading below. See [core concepts](core_concepts.md) and [scene system](scene/scene.md) for callbacks and reset behavior.

## 4. Configuring the Application

The `ApplicationDescriptor` returned by `descriptor()` determines startup settings.

| Field | Default and purpose |
| --- | --- |
| `logical_width` | `1280`; logical image width, must be positive |
| `logical_height` | `720`; logical image height, must be positive |
| `initial_route` | Defaults to `SceneKeys::Invalid`; explicitly configure a valid route |
| `presentation` | Texture filtering, font source/size, default UI theme, and startup engine logo selection |

Logical resolution governs scene coordinates and logical drawing, not physical window pixels. See [configuration guide](content/configuration.md) for window/user settings and [application_presentation_settings.h](../../../engine/application/application_presentation_settings.h) for complete `presentation` fields/defaults.

### 4.1 Configuring the Initial Scene

The preceding example enters an empty scene directly. The repository example prepares project content through built-in `StartupLoading` before entering the main menu. This complete helper creates that route; its success route must target a registered game scene.

```cpp
#include "engine/builtin/builtin_scene_keys.h"
#include "engine/builtin/scenes/startup_loading_scene_payload.h"

#include <utility>

namespace initialization_example
{
elysia::scene::SceneRoute make_startup_route(
    elysia::scene::SceneRoute success_route)
{
    elysia::builtin::StartupLoadingScenePayload payload;
    // Enter resource-dependent scenes only after project content is ready.
    payload.success_route = std::move(success_route);
    payload.wait_for_logo_sequence = false;
    payload.wait_for_confirmation = true;
    return {
        .target = elysia::builtin::SceneKeys::StartupLoading,
        .payload = std::move(payload),
        .reload_mode = elysia::scene::SceneReloadMode::Reuse
    };
}
}
```

Define the helper before its use or declare it in a header and define it in an implementation file. Then set `descriptor.initial_route = make_startup_route({ .target = kEntry });` to enter `EntryScene` after content loading and confirmation.

The two wait settings match the repository example: successful loading need not wait for the logo sequence, but user confirmation remains required. The flow is:

```text
StartupLoading
    |
    +-- Load project content and activate project fonts
    |
    +-- Show confirmation prompt after success
    |
    +-- User confirms
    v
MainMenu
```

Set `wait_for_confirmation` to `false` to enter automatically after loading. Both wait settings govern only successful-flow waiting; they do not skip loading or failure handling. See [startup loading scene](builtin-scenes/startup-loading.md) for Payload, logos, and failures, and [resource loading](content/resources.md) for directories/manifests.

## 5. Registering Game Scenes

`register_scenes()` uses `register_game_scene<T>(key)`. The module above uses `kEntry`; real games can register menus, levels, and results using the same identifiers in routes.

Game `SceneKey` values range from `1` to `999`; duplicate registration is prohibited. The engine registers built-in scenes before calling the module, so do not register `StartupLoading` again.

Both initial and later targets must already be registered. With startup loading, `initial_route` must target the built-in loader and the module must register the game scene in its `success_route`. A valid number does not establish that a scene exists.

See [scene system](scene/scene.md) for constructor arguments, switching, payloads, and lifetime. Modules only register; scenes create objects and organize content.

## 6. Custom Development Panels (Optional)

Development overlays initialize, process events, and draw tools. Individual panels register through the overlay's `IDevelopmentPanelRegistry::register_panel()`. Creating an overlay does not itself add a custom panel.

The repository example overrides `create_development_overlay()` to return `ImGuiDevelopmentOverlay`. This local method implementation requires a matching `const override` declaration in the module; its enabled-ImGui branch also needs `engine/tools/imgui/imgui_development_overlay.h`.

```cpp
std::unique_ptr<elysia::tools::IDevelopmentOverlay>
GameModule::create_development_overlay() const
{
#if ELYSIA_ENABLE_IMGUI
    return std::make_unique<elysia::tools::ImGuiDevelopmentOverlay>();
#else
    return {};
#endif
}
```

The application calls this factory and acquires its overlay only when CMake option `ELYSIA_ENABLE_IMGUI` is enabled. When disabled, it does not call the method even if overridden. Keep the default empty implementation when tools are unnecessary.

The engine initializes, drives, and shuts down the overlay; modules should not call runtime lifecycle methods themselves. Custom implementations must honor contracts such as `shutdown() noexcept` and prevent exceptions escaping. See [development_overlay.h](../../../engine/tools/development_overlay.h) for the full interface and the repository module for working factory declarations/implementation.

See [development panels guide](tools/development-panels.md) for a complete registration/unregistration scene example.

## 7. Startup Failures and Troubleshooting

First preserve the console output, logs, and error messages from the failing run, then identify the stage where startup stopped. The following covers common integration problems. An error code identifies a failure category; the diagnostic entries that follow explain the specific cause.

### 7.1 Identify Where Startup Stopped

| Stage | What is being prepared | Where to look first |
| --- | --- | --- |
| Configuration preparation | Game module descriptor, project root, registry, application and user configuration | Console, startup dialog, configuration diagnostics |
| Platform initialization | SDL, audio device, font library, window, and GPU renderer | Logs categorized as `platform` or `audio`, and startup messages |
| Base resources and scene preparation | Development overlay, built-in resources, localization, fonts, preloads, scene registration | Logs categorized as `builtin`, `localization`, `typography`, `scene`, and related categories |
| Project content loading | Manifests and load plan, project configuration, textures, animations, audio, and fonts | `CONTENT-…` diagnostics and the startup loading failure screen |
| Entering the target scene | Wait conditions, success route, target scene construction and entry | Loading status, route configuration, original scene exception |

These are explanatory stages, not a shared state enum in the code. When the initial route uses `StartupLoading`, manifest parsing and request assembly begin in its `on_enter()`; later loading advances through the main loop. Successful `ELYSIA_INITIALIZE_APP` therefore does not mean project content has loaded successfully, or guarantee that the displayed scene is the intended game scene.

If loading succeeds but the startup screen remains visible, first check `wait_for_confirmation` and `wait_for_logo_sequence`. Waiting for confirmation or logo completion as configured is normal behavior; see the [startup loading scene](builtin-scenes/startup-loading.md).

### 7.2 Troubleshoot Common Errors by Stage

The English strings below are searchable error codes or log fragments, with dynamic paths, resource keys, and exception details omitted. “Abort initialization” means recording the failure, attempting to show a message, cleaning up, and ultimately returning `false`. Loading failure routes and scene error boundaries are explained in the next section.

#### Configuration Preparation

| Error code or log keyword | Common cause | Engine handling | Check first |
| --- | --- | --- | --- |
| `BOOTSTRAP-PROJECT-ROOT` | Searching upward from the startup path finds no project marker | Abort initialization | Check the searched paths in the diagnostic and `assets/.elysia_root`. The application passes the executable path when available; relative paths also depend on the working directory. Check the deployment directory structure as well as the IDE working directory. |
| `BOOTSTRAP-ASSET-DIRECTORY`, `BOOTSTRAP-PATH-ACCESS` | A required asset directory is missing or a path cannot be accessed | Abort initialization | Check the actual paths and permissions in the diagnostic. `assets/` must contain `audio/`, `textures/`, `fonts/`, and `configs/`; having these directories does not mean all resource files are present. |
| `BOOTSTRAP-RUNTIME-DIRECTORY`, `SAVE-INITIALIZE` | Runtime data directory creation or save service initialization failed | Abort initialization | Check permissions for `player_data/`, `player_data/saves/`, and `logs/` under the project root, and whether a file occupies a required directory path. Preserve saves and diagnostics. |
| `BOOTSTRAP-CONTENT-REGISTRY`, `BOOTSTRAP-APP-CONFIG` | Registry or application configuration is missing, contains invalid JSON, or violates the format | Abort initialization | Follow `bootstrap.app_config` in `assets/content_registry.json` to the actual file and inspect the field identified by the diagnostic's JSON Pointer. Keep startup application configuration distinct from manifests loaded later with project content. |
| `BOOTSTRAP-USER-CONFIG` | Reading, recovering, or writing user configuration encountered a problem | May recover with a warning or abort initialization | Check the log level and recovery entries, `player_data/user_config.json`, and recovery copies named in the diagnostic. A newer format version, access failure, or failure to write recovered settings cannot simply be ignored as ordinary corruption. |
| `Game module descriptor failed`, `Game module logical viewport must be positive`, `Application default UI theme is invalid` | `descriptor()` threw, logical dimensions are not positive, or the default theme is invalid | Abort initialization | Inspect the module descriptor and `presentation` settings. Dimensions must be positive and the theme must be supported. |

See the [configuration guide](content/configuration.md) for configuration relationships and the [bootstrap and registry reference](../reference/resources/bootstrap-and-registry.md) for registry fields. Path and configuration error handling is defined by [PathManager](../../../engine/io/path/path_manager.cpp) and [BootstrapFailure](../../../engine/bootstrap/bootstrap_types.h).

#### Platform Initialization

| Error code or log keyword | Common cause | Engine handling | Check first |
| --- | --- | --- | --- |
| `SDL3 Error`, `SDL_ttf Error` | SDL subsystem or font library initialization failed | Abort initialization | Check runtime dependencies, the system environment, and console output. Inspect the relevant initialization call when further diagnosis is needed; the summary message alone does not establish a specific device cause. |
| `SDL_mixer Error`, `AudioService initialization failed` | Mixer library, audio device, or track setup failed | Abort initialization | Check the system playback device, drivers, and runtime dependencies. This occurs before project audio loading; changing sound resource keys is not the first step. |
| `SDL_CreateWindow Error`, `SDL GPU renderer creation failed`, `SDL VSync configuration failed` | Window, GPU backend, or presentation settings are unavailable | Abort initialization; no automatic software-renderer fallback | Check the display environment, graphics drivers, GPU backend support, and user window settings. See the [build guide](build-and-run.md#running-tests) for device requirements. |
| `STARTUP-WINDOW` | Startup window settings could not be fully applied | Warn and continue, or abort initialization, depending on the result | Read the failed window operation and underlying cause. Distinguish settings that can fall back from required operations that failed; this code alone does not tell you whether the application exits. |

These messages come from [Application startup](../../../engine/application/application.cpp). They concern platform setup; failures to load project audio or font files belong to content loading.

#### Base Resources and Scene Preparation

| Error code or log keyword | Common cause | Engine handling | Check first |
| --- | --- | --- | --- |
| `Development overlay creation failed`, `Development overlay initialization failed` | The module's overlay factory or overlay initialization failed | Abort initialization | Check `create_development_overlay()`, the actual ImGui build option, and the custom implementation. See [development panels](tools/development-panels.md) for integration. |
| `Built-in asset initialization failed`, `Built-in music player initialization failed` | Required built-in assets, fonts, or built-in music playback facilities are unavailable | Abort initialization | Use the original error to locate the built-in resource or runtime dependency. Adding built-in keys to project manifests does not replace those resources. |
| `LOCALIZATION-MANIFEST`, `LOCALIZATION-LOCALE`, `LOCALIZATION-LANGUAGE` | Localization manifests, language files, or language configuration violate requirements | Abort initialization | Follow `manifests.required.i18n` in the registry and inspect the language manifest and files named in the diagnostic. Do not treat every language fallback as fatal. |
| Font diagnostics in the `typography` category | Invalid font source, point size, or font configuration required for a language | Abort initialization | Check `descriptor.presentation.fonts` and the specific diagnostic; the category alone is not the complete cause. Project font loading and activation also require the checks in the next table. |
| `BOOTSTRAP-PRELOAD` | The preload manifest or one of its textures failed to load | Abort initialization | Follow `bootstrap.preload_manifest` in the registry and check files and paths. Project logos must reference preload keys; declaring them only in a later project texture manifest is insufficient. |

#### Project Content Loading

The startup loading scene handles these failures. A visible screen does not make it safe to enter a game scene that depends on unfinished resources.

| Error code or log keyword | Common cause | Engine handling | Check first |
| --- | --- | --- | --- |
| `CONTENT-CONFIG`, `CONTENT-MANIFEST` | Invalid project configuration or content manifest | Stop loading, clear project content, then follow the loading failure route | Follow registry references to the declaration file named in the diagnostic and inspect the JSON Pointer. See the [resource reference](../reference/resources/README.md) for complete formats. |
| `CONTENT-PLAN` with `Duplicate` | Multiple declarations generate the same key in one resource registry | Reject the load plan and follow the loading failure route | Inspect both the first and duplicate declarations, including core manifests, additional modules, entity IDs, and namespaces. Different resource registries may use the same key. |
| `CONTENT-MISSING` | Source files are missing after requests are generated | Stop loading and follow the loading failure route | Inspect each `expected` and `declared_at` entry, including expanded template paths, filenames, animation segments, and frame numbers. Checking that a directory exists is not enough. |
| `CONTENT-TEXTURE`, `CONTENT-ATLAS`, `CONTENT-FONT`, `CONTENT-AUDIO` | File reading, decoding, or runtime resource creation failed | Clear loaded project content and follow the loading failure route | Check file format and validity using the specific resource key, path, and underlying cause. An existing file may still fail to decode. |
| `CONTENT-PLAN`, `CONTENT-ANIMATION`, `CONTENT-EFFECT` | Animation or effect assembly, validation, or registration failed | Stop loading and follow the loading failure route | Use the reason to check layouts, frame counts, segments, resource references, and effect mappings. The stage code alone does not identify the invalid field; see the [animation and effect reference](../reference/resources/animation-and-effects.md). |
| `Startup font activation failed` | Project fonts cannot be activated as configured after loading | Do not take the success route; follow the loading failure route | Check that the presentation settings' font sources, resource keys, and point sizes match the loaded fonts. Loading progress alone does not establish readiness for gameplay. |

See [ContentLoadFailure](../../../engine/loading/content_load_failure.h) and [GameContentLoader](../../../engine/loading/game_content_loader.cpp) for error codes and loading cleanup behavior.

#### Registering and Entering the Target Scene

Scene integration problems can occur near the end of initialization or during the success transition after loading.

| Error code or log keyword | Common cause | Engine handling | Check first |
| --- | --- | --- | --- |
| `Scene composition failed` with `duplicate SceneKey`, `outside the game range`, or `unregistered game SceneKey` | Duplicate registration, a reserved key, or an unregistered initial target | Composition exceptions during initialization abort initialization | Compare `register_scenes()` with `initial_route`. Game keys use `1–999`, but valid numbers still require registration. An unregistered target reached only after loading goes through the runtime error boundary; initialization need not detect it in advance. |
| `StartupLoadingScene requires StartupLoadingScenePayload`, `StartupLoadingScene requires a valid success route`, `StartupLoadingScene failure route is invalid` | Wrong loading-scene Payload type or invalid route | The entry callback throws an integration error, handled by the scene error boundary | Compare with this guide's route example and check the initial, success, and custom failure routes separately. |
| Original exception in the `scene` category | Target scene construction or `on_enter()` failed | Clean up through the scene error boundary and attempt error display; exit with failure if recovery is unavailable | Read the original cause and source. Check Payload, resource preparation order, and external subscriptions created on entry. Do not simply reenter the original scene after seeing an error screen. |

### 7.3 Understand What the Engine Has Already Done

**When initialization fails**, the application records the error, attempts to show a startup message, closes resources already established, and returns `false` from `ELYSIA_INITIALIZE_APP`. The entry point must return failure without calling `ELYSIA_RUN_APP`. Dialog display depends on the environment and error path and is not guaranteed.

**When startup content loading fails**, the loader stops further work and clears project content. The startup loading scene requests `failure_route` if provided, otherwise built-in `ApplicationFailure`. Custom failure routes do not automatically add diagnostic fields to arbitrary Payload types. Presentation failures such as font activation also enter the failure flow, but not every failure path performs the same loader rollback. See [failure and reentry](builtin-scenes/startup-loading.md#failure-and-reentry).

**Scene entry exceptions** are handled by the scene manager's error boundary. If cleanup succeeds and error display is available, the application may enter the failure scene; successful initialization can therefore mean it has reached an error-handling flow. Failed scene cleanup, another failure in the error scene, or an unusable rendering backend does not trigger unlimited retries. Reaching an error screen does not restore game state; see the [application failure scene](builtin-scenes/application-failure.md).

**Warnings can accompany successful startup.** For example, if startup presentation finds an empty optional project logo key or an unavailable texture, it logs a warning and skips the logo. This does not waive preload manifest requirements: failure to load a file listed there still prevents initialization. The required engine startup logo is also outside this optional fallback.

After user configuration is corrupted, successfully archiving it and recovering a valid copy or rebuilding from application defaults may produce only a `BOOTSTRAP-USER-CONFIG` warning. Having no configuration on the first run normally leads to creation, not failure. An unreadable file, a newer format version, or failure to write recovered settings may block startup. Preserve original diagnostics and recovery files, respond to the actual result, and do not use configuration deletion as a universal fix.

### 7.4 Read the Complete Diagnostic

Default file logs live in **`logs/` under the identified project root**. Each run creates a file resembling `Elysia-date-time-processID.log`; logs are not necessarily beside the executable. Custom `LoggerConfig` can change the file mode or disable file output.

Console logging starts before file logging. The application initializes file logging only after startup configuration parsing succeeds. Early directory or configuration failures may leave no log file for that run, and a failed file open also prevents file output. In that case, launch from a terminal and preserve console output and startup messages. A file opened later does not backfill early console messages. Release startup messages and error screens may omit raw diagnostics; a short onscreen message is insufficient to locate the cause.

The following is an **illustrative diagnostic reading example**, not a captured run. The resource key, missing file, and source line number are hypothetical:

```text
CONTENT-MISSING [startup]
A texture source file is missing
[1] type=texture key=ui.example
    expected=assets/textures/ui/example.png
    declared_at=assets/configs/manifests/textures_manifest.json#/textures/ui.example/path
    reason=File does not exist
    source=resource_load_plan_preflight.cpp:123
```

Use the error code and category to identify the loading stage, then use `key` to identify the logical resource. `expected` names the file that must exist; `declared_at` identifies the declaring configuration, and the JSON Pointer after `#` locates the field. Here it means the `path` field of `ui.example` inside the `textures` object. `reason` explains the failure; `source` is the code location that produced the diagnostic, not a JSON line to edit. Duplicate keys and similar problems produce multiple entries; preserve the first declaration, duplicate declaration, and any additional failures together.

Not every diagnostic supplies every field, and paths may be shortened relative to the project root. When reporting a problem, preserve the complete message and relevant entries rather than only the final “startup failed” line. See the [utilities guide](tools/utilities.md#logging) for logging APIs and [FailureDiagnostic](../../../engine/core/diagnostics/failure_diagnostic.cpp) for formatting.

## 8. Complete Reference Implementation

The empty-scene module illustrates minimal wiring without all repository game settings. To inspect actual startup, continue with:

| File | Contents |
| --- | --- |
| [main.cpp](../../../main.cpp) | Entry point, runtime result, exception fallback |
| [game_module.h](../../../engine/application/game_module.h) | Module interface and application descriptor |
| [example_game_module.h](../../../game/application/example_game_module.h) | Three example module interface declarations |
| [example_game_module.cpp](../../../game/application/example_game_module.cpp) | Presentation settings, loading route, scene registration, overlay factory |

After integrating a module, read the [GameplayScene guide](gameplay/scene.md) to organize gameplay scenes.
