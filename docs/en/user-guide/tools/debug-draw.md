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

## References

- [Primitives and categories](../../../../engine/tools/debug_draw.h)
- [Development overlay](development-panels.md), [physics](../systems/physics.md)
- [Back to user guide](../README.md)
