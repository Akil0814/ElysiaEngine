# Integrating Development Panels

Development overlays serve tools such as Inspector; production menus/HUD use the [UI system](../ui/README.md#create-window). Enable the build option, have the module create an overlay, then register panels from scenes.

## Enabling the Overlay

Repository development builds currently enable it by default. Explicitly disable it to remove tools entirely from a release build:

```powershell
cmake -S . -B build -DELYSIA_ENABLE_IMGUI=OFF
```

Disabled builds create no `imgui_lib`, compile neither adapters nor the game Physics Inspector, and link no ImGui symbols. Compile conditions also remove `Application` event/frame/render integration; `SceneRuntimeContext::development_panels()` always returns null. Such builds have no ImGui context, font atlas, event handling, statistics collection, or drawing cost.

Explicitly enable it or fix an existing cache:

```powershell
cmake -S . -B build-imgui -DELYSIA_ENABLE_IMGUI=ON
cmake --build build-imgui --config Debug
```

Enabled builds use pinned Dear ImGui 1.92.9, `imgui_impl_sdl3`, and `imgui_impl_sdlrenderer3`. They do not compile `imgui_demo.cpp` or enable docking, multiple viewports, or gamepad navigation. Hidden overlays retain initialized context/default font memory but send no backend events and run no `NewFrame`, panel callbacks, or rendering.

Changing `option()` defaults does not override existing `CMakeCache.txt`; reconfigure old directories explicitly with `-DELYSIA_ENABLE_IMGUI=ON`.

The module must also return the adapter from `create_development_overlay() const`; enabling the option does not create one for the project. See [game initialization](../game-initialization.md#6-custom-development-panels-optional) for the factory. The application initializes/closes the overlay; games do not drive its frame loop.

## Registration and Unregistration

This complete scene only demonstrates wiring. Drawing inside the callback compiles only with ImGui enabled; the scene still enters normally when disabled. `imgui.h` is confined to this development scene.

```cpp
#include "engine/scene/scene.h"
#include "engine/tools/development_overlay.h"
#include "engine/tools/logger.h"
#if ELYSIA_ENABLE_IMGUI
#include <imgui.h>
#endif

class DevelopmentPanelScene final : public elysia::scene::Scene
{
public:
    ~DevelopmentPanelScene() override
    {
        // Applications destroy scenes before overlay shutdown; this is fallback beyond normal exit.
        detach_panel();
    }

protected:
    void on_enter(const elysia::scene::ScenePayload&) override
    {
        detach_panel(); // Do not duplicate stable IDs on reentry.
        _panels = runtime_context().development_panels();
        if (!_panels)
        {
            return; // No overlay or disabled at build time is normal.
        }
        _panel = _panels->register_panel("project.example.inspector", [this]
        {
            draw_inspector();
        });
        if (!_panel.is_valid())
        {
            ELYSIA_LOG_ERROR("development", "Failed to register inspector panel.");
        }
    }

    void on_exit() override
    {
        detach_panel();
    }

    void on_reset() override
    {
        detach_panel();
    }

private:
    void detach_panel()
    {
        if (_panels && _panel.is_valid())
        {
            // This example never unregisters or destroys scenes inside panel drawing.
            (void)_panels->unregister_panel(_panel);
        }
        _panel = {};
        _panels = nullptr;
    }

    void draw_inspector()
    {
#if ELYSIA_ENABLE_IMGUI
        if (ImGui::Begin("Example inspector"))
        {
            ImGui::Text("Scene paused: %s", is_paused() ? "yes" : "no");
        }
        ImGui::End(); // Pair End even when Begin returns false.
#endif
    }

    elysia::tools::IDevelopmentPanelRegistry* _panels = nullptr;
    elysia::tools::DevelopmentPanelHandle _panel{};
};
```

Panel stable IDs are unique among active panels within an overlay lifetime. Empty IDs/callbacks and duplicates return invalid handles and record diagnostics without throwing. Handles increase monotonically and are never reused. Registration/unregistration during drawing queues changes; the current batch uses its entry snapshot, and changes take effect next frame.

Panel callbacks are caller code and must not throw. Exceptions propagate to the `Application` render boundary, record `UnhandledException`, and cause FaultExit. The engine does not roll back arbitrary ImGui or scene state.

Callbacks borrow scenes, so unregister before destroying captured objects. During drawing, unregistration is deferred and the current snapshot may still contain callbacks. Do not immediately release captured objects during panel drawing. The example unregisters at scene lifecycle boundaries.

## Display and Debugging

F2 toggles the overlay. Hidden overlays retain context without callbacks. Visible tools may capture mouse/keyboard; gamepads still reach the game. This does not pause physics.

All three Physics Combat Demos register `physics_demo.inspector` through `GameplayDemoSceneBase`. F2 toggles the overlay; F1 retains its `DebugDraw` toggle. Inspector displays:

- Frame delta, ImGui FPS, and scene name;
- World settings: fixed step, gravity, maximum catch-up steps per frame, Box2D `sub_steps`;
- Latest fixed-step statistics: registered objects, colliders, contacts, awake bodies, joints, physics step time, and accumulated dropped fixed steps;
- Shapes, Contacts, and Velocities counts from the current on-demand debug snapshot;
- Global `DebugDraw` toggle and categories for render-pose colliders, contacts/normals, Native AABB, previous/current physics poses, velocities, joint anchors, and Gameplay.

Maximum catch-up limits scene fixed steps per frame; `sub_steps` is Box2D subdivision within one physics simulation, not the same setting or old solver iterations. Native AABBs/contacts come from the latest physics step; collider outlines use render poses. Pose history compares simulation states and diagnoses interpolation. Joint anchors mark connections on bodies and also use interpolated poses to align with rendering. See [physics reference](../../reference/physics/units-and-state.md) for units/debug state.

Checkboxes directly modify existing `DebugDraw`; the next scene update maps them to `PhysicsDebugCapture`. Inspector creates no second debug state and changes no simulation results.

## Boundaries

No layout persistence, custom fonts, remote debugging, ImPlot, multiple windows, or Viewport is provided. Games own panel data; callbacks must neither implement required per-frame game logic nor throw.

See [development overlay architecture](../../architecture/subsystems/development-overlay.md) for ownership, capture, and render state restoration; see [DebugDraw](debug-draw.md) for primitives.
