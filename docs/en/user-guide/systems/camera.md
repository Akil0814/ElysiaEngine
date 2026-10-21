# Cameras and Coordinates

This guide covers scene integration, following, conversions, and camera motion. Scenes operate declared owned slots through their runtime; drawing uses the final presentation view.

Enable optional `CameraSceneConfig`, configure owned slots with `camera_runtime()`, and read final view through `Scene::camera()`. Standard scenes update cameras each frame; do not drive services again.

## Configuring Following

This complete scene explicitly enables a camera and configures following on every entry. `SceneManager` synchronizes viewport size to the logical canvas; scenes do not override it separately. Replace the fixed focus rectangle with target `render_rect()` in real games.

```cpp
#include "engine/scene/scene.h"
#include "engine/camera/follow_strategy.h"

#include <memory>

class FollowScene final : public elysia::scene::Scene
{
public:
    FollowScene()
        : Scene(elysia::scene::SceneRuntimeFeatures{
            .camera = elysia::scene::CameraSceneConfig{
                .focus_mode = elysia::scene::CameraFocusMode::ResolveEachFrame}})
    {
    }

protected:
    void on_enter(const elysia::scene::ScenePayload&) override
    {
        using namespace elysia::camera;
        // Borrow this scene's camera runtime; the scene advances it without manual updates.
        elysia::scene::SceneCameraRuntime& cameras = camera_runtime();
        cameras.set_zoom(CameraSlot::Main, 1.0f);
        cameras.set_world_bounds(CameraSlot::Main,
            elysia::core::Rect{0, 0, 3000, 2000});
        cameras.set_follow_strategy(CameraSlot::Main,
            std::make_unique<SmoothFollowStrategy>(300.0));
    }

    // No external subscriptions/business state; scene lifetime cleans camera activity.
    void on_exit() override
    {
    }

    void on_reset() override
    {
    }

    std::optional<elysia::camera::CameraFocus> resolve_camera_focus(
        elysia::camera::CameraSlot) const override
    {
        // Single target: bounds and primary share a rectangle; geometry is copied.
        const elysia::core::Rect target{100, 100, 32, 32};
        return elysia::camera::CameraFocus{target, target};
    }
};
```

Override `resolve_camera_focus(CameraSlot)` and provide target `render_rect()` as both `CameraFocus` rectangles, or `std::nullopt` without a valid target. Declare slots in `CameraSceneConfig::owned_slots` and choose `CameraFocusMode::ResolveEachFrame`. Each owned slot resolves/advances independently. Clear borrowed target pointers on removal.

## Common Operations

| Task | Interface/behavior |
| --- | --- |
| Fixed view | `set_center(slot, position)` with `set_follow_strategy(slot, nullptr)` |
| Direct follow | `HardFollowStrategy` |
| Constant-speed follow | `SmoothFollowStrategy(speed)`, world units/second |
| Dead-zone follow | `DeadZoneFollowStrategy(rect)`, viewport-local logical coordinates |
| Map bounds | `set_world_bounds(slot, rect)`; nullopt removes |
| Immediate zoom | `set_zoom(slot, value)` cancels slot pose motion |
| Smooth movement/zoom | `camera_runtime().move_to(slot, CameraPoseTarget{...}, seconds)` |
| Multi-node path | `camera_runtime().play_path(slot, CameraMotionSpec{...})` |
| Shake | `camera_runtime().request_shake(slot, CameraShakeParams{...})`, amplitude/duration_seconds/frequency_hz |
| Snap/clear effects | `camera_runtime().snap_to_focus(slot)`, `clear_effects(slot)` |

Setters apply directly; shake requests process next update. Pose motion starts at current logical center/zoom; node durations must be finite positive. Center/zoom may be specified independently; omitted channels retain segment-start values. Default natural completion resumes follow; `CameraMotionEndBehavior::Hold` stays at destination. `clear_effects` clears transient effects such as shake without cancelling pose motion.

## Multiple Targets and Slots

Include `engine/camera/multi_target_follow_strategy.h` and use `std::make_unique<elysia::camera::MultiTargetFollowStrategy>()`. Resolve arrays per slot and return `elysia::camera::make_camera_focus(rects, primary_index)`. This copies geometry without retaining objects; empty/all-invalid returns nullopt.

`MultiTargetFollowConfig` includes safe_ratio, inner_ratio, min_zoom/max_zoom, movement_half_life, zoom_out_half_life, zoom_in_half_life, settle_seconds, dead_zone_enabled. Require `0 < inner_ratio < safe_ratio <= 1`. Pass via construction/`set_config`; the latter resets timers. Smaller half-lives respond faster; settle_seconds delays zoom-in after gathering.

Multi-target follow adjusts position/zoom, with delayed zoom-in and faster zoom-out. If minimum zoom cannot fit all, prioritize primary. Following/automatic zoom yield during pose motion and resume next frame after ResumeFollow completion. Smoothing, bounds, and shake may temporarily leave targets offscreen; not every frame contains all targets.

Fixed slots: Main, Cinematic, Auxiliary1, Auxiliary2. `initial_slot`/`owned_slots` declare presentation/ownership. `cut_to(slot)` switches immediately; `blend_to(slot, spec)` fixes source composition while continuously reading live target. Multiple slots do not provide split-screen layout.

Paths/blends return handles for query/pause/resume/cancel. Natural completion emits value hooks `on_camera_motion_completed` / `on_camera_blend_completed`; lifecycle cancellation never masquerades as completion. Interrupting a blend continues from actual displayed composition without jumping back.

## Coordinate Conversion

`world_to_screen(point/rect)` returns viewport-local logical rendering coordinates; `screen_to_world` reverses. Visible world size is `viewport_size / zoom`; rectangles transform position/size.

`InputSystem` first converts window to logical coordinates. Convert those logical pointer positions with the current rendering camera's `screen_to_world` for aiming. Projection is neither physical window pixels nor container-local coordinates. UI bypasses world cameras.

Project `render_rect()` for displayed positions; game decisions still use `world_rect()`. Do not preproject world drawing commands and cause double conversion.

## Switching and Common Misuse

Switching scenes or Reset/Recreate resets only target owned slots, retaining viewport but clearing focus/bounds/strategy/motion/effects. Reuse still exits/enters and cancels unfinished playback while retaining static pose/configuration. Shutdown resets all slots.

Recreate constructs candidates before exit/cache destruction/activation. Configure in `on_enter`, never candidate constructors touching shared slots. Exit cancels without natural completion. `on_after_update` follows camera advancement: direct pose changes can affect current rendering, but new motions consume time next advancement.

No-camera scenes still update/render UI; world commands or visible world `DebugDraw` cause Render boundary contract errors. Manual control/following may interact; choose control appropriately. Requests are main-thread-only.

## Camera Showcase

Main menu → module showcase → camera (SceneKey 13) has three tabs:

- **Following and framing**: hard follow, 480 units/second smooth follow, central 40% single-target dead zone, multi-target; WASD primary, Q/E separate secondary. Includes primary switching, auto separation/reunion, teleport, zoom/bounds presets.
- **Camera motion**: move/zoom or three-segment paths; Linear, SmoothStep, EaseInOutCubic, ResumeFollow/Hold. Pause/resume/cancel/follow/shake share scene controls. Completed Hold handles remain queryable/cancellable. Paused pose motion also freezes slot shake until resumed.
- **Cinematic camera**: cut/blend to Cinematic or play wide-view entry, path, zoom-in, hold/shake, return Main. Actor/automatic motion freeze during takeover while control bindings remain; return restores automatic motion choices. Pause/resume/skip/replay work through stages; standalone shake is disabled during blends.

F1–F3 switch tabs; R resets current. Tabs/buttons support mouse/focus/confirm. Esc/Back return directly and clean camera activity. Switching initializes pages; entering cinematic retains previous automatic motion choice until takeover. UI uses existing `UiTabBar`/scrolling action rows; world overlays clip to content.

Grid/axes reveal composition; yellow nodes/lines show paths, purple boxes cinematic landmarks. Blue indicates multi-target safe region, green dead zone, red bounds, white primary. Status shows strategy, center, zoom, motion, cinematic stage, and source-to-target direction during blending.

## References

- [Scenes](../scene/scene.md), [input](input.md), [scene camera runtime](../../../../engine/scene/runtime/scene_camera_runtime.h)
- [Optional architecture](../../architecture/subsystems/camera.md), [back to user guide](../README.md)
