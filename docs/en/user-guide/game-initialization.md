# Game Project Initialization Guide

This guide explains the responsibilities and connections of the program entry point, game module, application configuration, and scene registration. You should already be able to build the repository using the [build and run guide](build-and-run.md).

The entry point hands execution to the engine. The game module supplies configuration and registers game scenes; the engine initializes subsystems and drives scenes. After explaining these roles, this guide gives minimal module and startup-loading route examples. They use the repository's CMake include paths; module code compiles, but execution still requires valid project configuration and resources.

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

`EntryScene` adds no visible content and loads no project content. Direct entry verifies module wiring. Scenes depending on project resources/fonts should use startup loading below. See [core concepts](core_concepts.md) and [scene system](scene/scene.md) for callbacks and reset behavior.

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

## 7. Complete Reference Implementation

The empty-scene module illustrates minimal wiring without all repository game settings. To inspect actual startup, continue with:

| File | Contents |
| --- | --- |
| [main.cpp](../../../main.cpp) | Entry point, runtime result, exception fallback |
| [game_module.h](../../../engine/application/game_module.h) | Module interface and application descriptor |
| [example_game_module.h](../../../game/application/example_game_module.h) | Three example module interface declarations |
| [example_game_module.cpp](../../../game/application/example_game_module.cpp) | Presentation settings, loading route, scene registration, overlay factory |

After integrating a module, read the [GameplayScene guide](gameplay/scene.md) to organize gameplay scenes.
