# Camera Architecture and Runtime Flow

This document explains how global slots and scene runtimes divide camera responsibilities, and how following, manual motion, and final presentation cooperate. See the [camera guide](../../user-guide/systems/camera.md) for game integration examples.

## Module Structure

The camera module has three layers:

```text
CameraManager (global sole owner and write entry point)
  ├─ Main       Camera + CameraController
  ├─ Cinematic  Camera + CameraController
  ├─ Auxiliary1 Camera + CameraController
  └─ Auxiliary2 Camera + CameraController

SceneCameraRuntime (optional scene capability)
  └─ owned slots, final presentation slot, blending state, and scene handle ownership
```

- `Camera` stores the final rendering center, screen viewport size, and `zoom`, and converts between world and screen coordinates.
- `CameraController` stores the logical center, focus, world bounds, follow strategy, shake, and single-slot pose motion.
- `CameraManager` owns exactly four camera groups and offers no dynamic creation or destruction.
- `SceneCameraRuntime` stores the final presentation slot and optional inter-slot blend state, restricting scene operations to declared owned slots and handles.
- `Scene` coordinates only focus resolution, lifetime, and completion event dispatch; it stores no camera playback state.

The four slots are:

| Slot | Purpose |
| --- | --- |
| `Main` | The scene's default world camera. |
| `Cinematic` | Cutscene and presentation views. |
| `Auxiliary1` | Extension camera with no predefined purpose. |
| `Auxiliary2` | Extension camera with no predefined purpose. |

## Access Rules

`CameraManager` is the underlying camera service. The camera subsystem may use it directly; scene code should use `camera_runtime()` for slots declared as owned by that scene. External callers can obtain only `const Camera&` for rendering, visible-area queries, and projection:

```cpp
const auto& camera = elysia::camera::CameraManager::instance()->camera(
    elysia::camera::CameraSlot::Main
);
```

`Camera` state must be modified through Manager:

```cpp
auto* cameras = elysia::camera::CameraManager::instance();

cameras->set_viewport_size(CameraSlot::Main, { 1280.0f, 720.0f });
cameras->set_zoom(CameraSlot::Main, 2.0f);
cameras->set_world_bounds(CameraSlot::Main, world_bounds);
cameras->set_follow_strategy(
    CameraSlot::Main,
    std::make_unique<SmoothFollowStrategy>(300.0)
);
```

Configuration takes effect immediately, including center, viewport, `zoom`, focus, bounds, and follow strategy. Setting center or `zoom` directly cancels that slot's active pose motion and synchronizes `Controller` with the final `Camera`, avoiding simultaneous writes by two control sources.

## Viewport, Zoom, and Coordinates

`viewport_size` uses screen units in logical rendering coordinates. `zoom` defaults to `1.0` and has a valid range of `[0.1, 10.0]`. Ordinary out-of-range values are clamped; non-finite values fall back to `1.0`. The camera's visible world size is:

```text
world_viewport_size = viewport_size / zoom
```

`world_to_screen` and `screen_to_world` support both points and rectangles. Rectangle conversion transforms position and size. Screen coordinates are local to the camera viewport, excluding window position and UI layout offsets. UI commands do not pass through these conversions.

## Requests and Update Order

Shake, immediate focus snapping, and effect clearing are FIFO transient requests. Center and `zoom` animation use unified pose motion:

```cpp
cameras->request_shake(CameraSlot::Main, shake_params);
cameras->request_snap_to_focus(CameraSlot::Cinematic);
cameras->request_clear_effects(CameraSlot::Main);

const auto motion = cameras->move_camera_to(
    CameraSlot::Main, {.center = destination, .zoom = 2.0f}, 0.5);
```

Requests enter a single-threaded FIFO queue in submission order. `CameraManager::update(slots, delta)` executes as follows:

1. Process all pending requests in FIFO order.
2. Update the caller's specified `CameraSlotSet`.
3. `Controller` advances active pose motion, temporarily taking control of center and `zoom` during playback.
4. Without pose motion, `Controller` applies the center and optional `zoom` returned by the follow strategy.
5. Enforce world bounds at the current `zoom`, then add shake offset.
6. Write the final center to `Camera` and return motion value events that completed naturally this frame.

Each slot stores one pose motion and one shake, which can run together. Path nodes use positive durations from the previous pose to the target and a shared easing curve. New motion continuously replaces old motion from the current logical pose. Clearing effects stops shake without cancelling pose motion. Direct pose changes, focus snapping, or explicit cancellation end motion without producing natural completion events.

DeadZone uses camera viewport-local screen pixels. Focus is projected using the current `zoom` before comparison, so `zoom` changes do not change the dead zone's visual screen size.

## Scene Integration

Scenes own neither `Camera` nor `CameraController`. Cameras are optional runtime capabilities; base scene behavior is:

- A scene declares camera runtime through optional `CameraSceneConfig`. Neither default `Scene` nor `GameplayScene` has a camera. All owned slots advance; `ResolveEachFrame` additionally calls `resolve_camera_focus(slot)`.
- Scenes with cameras project world commands through the final presentation camera. Scenes without cameras can render UI normally, but submitting world commands or visible world `DebugDraw` is a configuration error.
- UI commands still execute directly in screen coordinates, bypassing the world camera.
- `Scene::camera()` returns the final presentation camera: the scene's composite camera during active blending, otherwise the committed slot.

```cpp
std::optional<elysia::camera::CameraFocus> MyScene::resolve_camera_focus(
    elysia::camera::CameraSlot slot) const
{
    if (slot != elysia::camera::CameraSlot::Main || !_player)
        return std::nullopt;
    const auto rect = _player->render_rect();
    return elysia::camera::CameraFocus{rect, rect};
}
```

A scene that needs player and cinematic cameras to update concurrently can configure:

```cpp
MyScene::MyScene() : Scene(elysia::scene::SceneRuntimeFeatures{
    .camera = elysia::scene::CameraSceneConfig{
        .initial_slot = elysia::camera::CameraSlot::Main,
        .owned_slots = elysia::camera::CameraSlot::Main
                     | elysia::camera::CameraSlot::Cinematic,
        .focus_mode = elysia::scene::CameraFocusMode::ResolveEachFrame}}) {}
```

`camera_runtime().cut_to()` commits a slot immediately. `camera_runtime().blend_to()` captures the current actual composition as a fixed source and interpolates each frame toward the live target slot. Center and `zoom` interpolate; the viewport comes from the target slot. A new blend continues from the current composite camera. Pausing freezes controllers and blending by default; configuration can allow them to keep advancing.

## Scene Switching and Reset

When switching to another scene or reentering the current scene with `SceneReloadMode::Reset` / `Recreate`, `SceneManager` resets all owned slots of the target scene after the old scene's `on_exit()` and before the new scene's `on_enter()`.

Slot reset:

- Clears focus and world bounds;
- Clears follow strategy, active motion, and effects;
- Clears pending requests for that slot;
- Zeros logical and final centers;
- Restores `zoom` to `1.0`;
- Preserves viewport size.

Slots not declared owned are unaffected by the switch. Reuse of the current instance preserves static pose, but exit still cancels active motions and blends. `SceneManager` shutdown resets every slot.

`Scene` exit and destruction cancel only motion handles recorded by that `SceneCameraRuntime`, rather than cancelling by slot and affecting motion created later by another scene. Delayed destruction of cached scenes therefore cannot stop a new trajectory in the current scene's same slot. Whole-slot reset occurs only in target-entry reset and application shutdown. `Recreate` constructs a candidate first, then exits the current scene, destroys the target's old cache, and activates the candidate. Constructors should not modify shared cameras; configure them in `on_enter`.

## Current Boundaries

- The request queue is main-thread-only and provides no synchronization.
- All four cameras start with zero center and viewport; applications or scenes must set a viewport explicitly.
- Dynamic cameras, split-screen viewport layout, auxiliary camera occupancy arbitration, and a general multi-effect stack are not currently provided.
- Rotation and clipping are future extensions of `Camera` and rendering projection, not follow strategies.
- `InputSystem` does not automatically convert pointer positions to world coordinates; application code must explicitly call `screen_to_world` on the relevant slot.

## Multi-Target Composition

`MultiTargetFollowStrategy` smoothly adjusts center and `zoom` from a group of rectangles. It accepts geometry values only and holds no game objects. After movement and physics interpolation, the scene obtains presentation rectangles from objects' `render_rect()` and supplies them to the strategy.

The following is a local fragment in a derived scene: configure the strategy once on entry and return current targets from the focus callback each frame. `_player` and `_companion` must point to live objects.

```cpp
#include "engine/camera/multi_target_follow_strategy.h"

// Configure on scene entry.
auto& cameras = camera_runtime();
cameras.set_follow_strategy(
    elysia::camera::CameraSlot::Main,
    std::make_unique<elysia::camera::MultiTargetFollowStrategy>());

std::optional<elysia::camera::CameraFocus> MyScene::resolve_camera_focus(
    elysia::camera::CameraSlot slot) const
{
    if (slot != elysia::camera::CameraSlot::Main) return std::nullopt;
    const std::array rects{_player->render_rect(), _companion->render_rect()};
    return elysia::camera::make_camera_focus(rects, 0); // Player is primary.
}
```

The same callback may return different focus for each owned slot. `CameraFocus::bounds` stores target bounding rectangles and `primary` selects the `primary` target. Empty or entirely invalid lists return `nullopt`. Non-finite rectangles are skipped; an invalid `primary` index falls back to the first valid rectangle. Point and line targets are supported; direct `set_focus()` also rejects non-finite rectangles.

### Configuration and Zoom Decisions

| Setting | Default | Behavior |
| --- | --- | --- |
| `dead_zone_enabled` | `true` | Move only enough to bring targets into the safe region; follow the group center when disabled |
| `safe_ratio` | `0.70` | Centered safe region as a fraction of viewport width/height |
| `inner_ratio` | `0.55` | Smaller region used to begin the `zoom`-in wait |
| `min_zoom`, `max_zoom` | `0.5`, `2.0` | Automatic `zoom` range; smaller `zoom` shows more world |
| `movement_half_life` | `0.12 s` | Time to halve center error for a fixed target |
| `zoom_out_half_life` | `0.10 s` | Faster response when zooming out |
| `zoom_in_half_life` | `0.35 s` | Slower response when zooming in |
| `settle_seconds` | `0.4 s` | Wait before zooming in or leaving `primary`-only mode |

Pass `MultiTargetFollowConfig` at construction or modify it with `set_config()`, which resets timers and mode. Ratios satisfy `0 < inner_ratio < safe_ratio <= 1`; `zoom` also obeys `Camera`'s global range. Invalid half-lives or wait times use defaults.

The strategy computes the `zoom` needed to fit group width and height inside the safe region. Required `zoom`-out starts smoothing immediately; `zoom`-in waits until targets remain inside the inner region for the specified time, then advances toward the `zoom` that fits the safe region. New `zoom`-out needs cancel `zoom`-in. Movement uses the blended `zoom` from the same frame; automatic following does not restart manual `zoom` animation.

If the group cannot fit even at `min_zoom`, the camera approaches that `zoom` and follows the `primary` target. Group composition resumes only after the group fits inside the inner region at that `zoom` for the settle time. Query this state with `primary_only()`. If the `primary` target itself exceeds the visible region, axes that cannot fit are centered on it.

### Manual Camera Motion and Lifecycle

```cpp
const auto motion = cameras.move_to(
    elysia::camera::CameraSlot::Main, {.zoom = 1.5f}, 1.0);
```

Active pose motion controls the camera through its completion frame. Omitted target channels retain their values from the segment's start. Following and automatic `zoom` resume on the frame after a `ResumeFollow` motion ends. Immediate `set_zoom()` takes effect at once; automatic `zoom` resumes on the next update. Values outside the automatic range return smoothly without a jump.

Both initial acquisition and reacquisition are smooth. Missing focus preserves composition and resets timers; explicit manual effects can still advance. Replacing the strategy resets its state. Explicit snap requests still align immediately to group bounds. Existing strategies retain immediate first acquisition and do not automatically change `zoom`.

`IFollowStrategy::update()` is non-const and returns `CameraFollowResult` with center and optional `zoom`. Custom strategies must migrate from old `update_center()`. When no pose motion controls the camera, strategies can use `automatic_zoom_enabled`; `reset()` clears state. `snap_on_acquisition()` defaults to `true`; the multi-target strategy overrides it to false.

During rapid separation or teleportation, smooth following may temporarily leave targets offscreen. World bounds take priority over composition, and shake is added afterward; neither guarantees all targets remain visible. Invalid viewports and non-positive or non-finite time steps do not advance this strategy.

### Demonstration and Validation

Enter the camera module from the module showcase and choose the multi-target strategy on the follow page. WASD moves the `primary` target; Q/E moves the other object horizontally. Controls cover dead zone, `primary` switching, automatic separation/reunion, world bounds, teleporting, manual `zoom`, and reset. White outlines mark the `primary`, blue the safe region, green the inner region, gray the group bounds, and red optional world bounds. Status shows `zoom` and composition mode without requiring ImGui.

Run `ctest --test-dir out/build/sdl3-Debug -C Debug -L camera --output-on-failure` to validate camera behavior. `demo_scene_tests` covers demonstration controls, return routing, and reentry; set `ELYSIA_CAMERA_QA_DIR` to export PNGs for layout inspection. Screenshots do not replace interaction in an actual window.
