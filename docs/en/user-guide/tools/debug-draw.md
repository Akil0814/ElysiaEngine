# Debug Drawing

This guide displays diagnostic regions/primitives and explains cleanup ownership. Debug drawing visualizes only; it changes neither collisions nor game rules.

`ELYSIA_DEBUG_DRAW` collects diagnostics for scene rendering. It creates no game objects or colliders and does not replace production art rendering. Both global and category enablement are required.

## Drawing Gameplay Regions

A scene update can call this function after explicitly configuring `CameraSceneConfig`; world primitives cannot render without a camera. See [camera guide](../systems/camera.md). The example assumes the scene centrally owns Gameplay-category drawing, clears that category, then submits the frame's complete primitives. Multiple modules must not independently clear the same category.

```cpp
#include "engine/tools/debug_draw.h"

void show_interaction_area(const elysia::core::Rect& world_rect)
{
    using elysia::tools::DebugDrawCategory;
    elysia::tools::DebugDraw* draw = ELYSIA_DEBUG_DRAW;
    draw->set_enabled(true);
    // Preserve other enabled categories and add only Gameplay for this example.
    draw->set_enabled_categories(
        draw->enabled_categories() | DebugDrawCategory::Gameplay);
    // This owns every primitive in the category; clear before submitting current regions.
    draw->clear_categories(DebugDrawCategory::Gameplay);
    draw->draw_rect(DebugDrawCategory::Gameplay, world_rect,
                    elysia::core::Color{0, 255, 0, 255}, 2.0f);
}
```

Normally call enablement only when development settings change; it is included here to show prerequisites. Use `draw_rect`, `draw_circle`, `draw_line`, and `draw_point` for each primitive.

## Coordinates and Categories

Positions, rectangle sizes, and radii use world coordinates projected by the scene camera. Line width and point diameter use logical screen units independent of zoom, not guaranteed physical window pixels.

`set_enabled_categories` / `clear_categories` accept combinations. Each `draw_*` requires one valid category, not a multi-category mask. Disabled/invalid categories, non-finite coordinates, empty rectangles, and non-positive radii/widths/point sizes are ignored without boolean error results.

## Command Retention and Cleanup

Rendering once does not clear commands. Appending every frame without cleanup accumulates old primitives. Use `clear_categories()` or centralized `clear()` with explicit ownership. Disabling globally clears commands; disabling a category clears its existing commands. Scene switches also clear them.

Scene updates refresh Physics categories from physics debug snapshots. Use Gameplay or other suitable categories for game primitives, rather than expecting hand-written Physics primitives to persist. Scenes draw debug primitives after world objects and before ordinary UI.

The Dear ImGui overlay may supply a debug panel, but `DebugDraw` is independent and needs no game-side ImGui drawing calls.

## Physics Detail Toggles

The global switch starts disabled. `DebugDrawCategory::Default` retains existing base categories, including physics outlines and anchors, while excluding the three new details. `All` includes every category. Physics examples start with `Default`; enable details separately in the Inspector or with shortcuts:

| Category | Example shortcut | Display |
| --- | --- | --- |
| `PhysicsJointDetails` | F2 | Eight joint types: axes, reference angles, active limits, springs and targets; Filter has no mechanical details |
| `PhysicsChainDetails` | F3 | Actual vertices, edges and open-chain endpoint ghosts; base outlines retain front arrows |
| `PhysicsTarget` | F4 | Kinematic target contours, orientation and current-to-target connection |

Shortcuts also work in no-ImGui physics examples, with a hint at the end of the scrolling status panel. Disabling a category removes retained commands immediately. Exit/reentry clears old geometry. Disabled, inactive, destroyed and pending objects are excluded from geometry/velocity snapshots. A joint is hidden if either endpoint is ineligible; sleeping bodies remain visible.

Joint anchors, local axes, limits and Chain vertices are rebuilt from interpolated body poses. Mouse targets stay in world coordinates; Motor local targets follow the first body. Reactions, contacts and measurements describe the latest physics state without display interpolation. Auxiliary axes span 0.25 meters in EU, angle arcs use a 0.2-meter radius, and arrowheads use 0.04 meters. Marker diameter and stroke width remain logical screen units.

Kinematic targets retain only pending-command and last-completed-step diagnostics. Pause/render-only frames retain them; the next fixed step without another target clears them. Post-step callback targets are marked pending. Later targets overwrite earlier ones in FIFO order; explicit velocity, angular velocity, teleport, disablement, removal and Reset clear the display. Fixed rotation retains the requested angle but draws the effective angle. Records do not drive persistent tracking. Targets use translucent cyan and ghosts use orange dashes.

For an independently owned world, use the shared category-to-capture mapping. Call this submission helper after that world's `finalize_frame`, with exclusive ownership of physics-category cleanup. Scene-owned physics already performs these steps; do not submit it twice.

```cpp
#include "engine/physics/physics_world.h"
#include "engine/physics/physics_debug_draw.h"

void enable_physics_details()
{
    using C = elysia::tools::DebugDrawCategory;
    auto* draw = ELYSIA_DEBUG_DRAW;
    draw->set_enabled(true);
    draw->set_enabled_categories(C::Default | C::PhysicsJointDetails |
        C::PhysicsChainDetails | C::PhysicsTarget);
}

void submit_owned_physics_debug(elysia::physics::PhysicsWorld& world)
{
    auto* draw = ELYSIA_DEBUG_DRAW;
    world.set_debug_capture(elysia::physics::physics_debug_capture_for(*draw));
    draw->clear_categories(elysia::physics::kPhysicsDebugCategories);
    elysia::physics::submit_physics_debug_snapshot(world.debug_snapshot(), *draw);
}
```

## References

- [Primitives and categories](../../../../engine/tools/debug_draw.h)
- [Development overlay](development-panels.md), [physics](../systems/physics.md)
- [Back to user guide](../README.md)
