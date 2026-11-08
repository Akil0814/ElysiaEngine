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

## CameraService from Objects and External Controllers

Include `engine/camera/camera_service.h` and use `ELYSIA_CAMERA`. It routes to the attached scene's camera runtime, so objects do not need a Scene pointer. Configuration, following, effects, cuts, blends and paths use the same parameters as the runtime. Viewport sizing remains application-owned.

```cpp
using namespace elysia::camera;
auto result = ELYSIA_CAMERA->request_shake({}); // Scene's default request slot.
if (!result) {
    const CameraServiceError reason = result.error();
    // For example, skip an optional effect when no camera runtime is active.
    (void)reason;
}
auto cinematic_shake = ELYSIA_CAMERA->request_shake(CameraSlot::Cinematic, {});
auto cut = ELYSIA_CAMERA->cut_to(CameraSlot::Cinematic);
auto blend = ELYSIA_CAMERA->blend_to(CameraSlot::Main, {.duration_seconds = 0.6});
```

`CameraSceneConfig::default_request_slot` optionally overrides the default shake slot. Otherwise it uses `initial_slot` (normally Main), even while presenting another slot. An explicit target must be owned by the scene. Shaking a hidden slot does not shake the final picture; the fixed-source blend continues to use its captured source composition.

`camera()` returns a value snapshot of the final presented view, including blending. `camera(slot)` returns a snapshot of that slot. `presented_slot()` names the source slot until a blend completes; it does not describe the interpolated pose. Queries and mutations return `std::expected<T, CameraServiceError>`; mutations without a value use `expected<void, ...>`. Errors distinguish NoActiveRuntime, InvalidSlot, SlotNotOwned, InvalidArgument, StaleOperation and OperationFinished. Existing zoom clamping and parameter validation still apply; unexpected failures propagate.

External choreography stores operation results and polls them from its update, after scene camera advancement:

```cpp
class CameraSequence {
    std::optional<elysia::camera::CameraBlendOperation> entering;
    std::optional<elysia::camera::CameraMotionOperation> moving;
public:
    std::expected<void, elysia::camera::CameraServiceError> start() {
        auto result = ELYSIA_CAMERA->blend_to(elysia::camera::CameraSlot::Cinematic);
        if (!result) return std::unexpected(result.error());
        entering = *result;
        return {};
    }
    std::expected<void, elysia::camera::CameraServiceError> update() {
        using namespace elysia::camera;
        if (entering && entering->status() == CameraOperationStatus::Completed) {
            auto result = ELYSIA_CAMERA->move_to(CameraSlot::Cinematic,
                {.zoom = 1.5f}, 1.0, CameraEasing::SmoothStep, CameraMotionEndBehavior::Hold);
            if (!result) return std::unexpected(result.error());
            entering.reset();
            moving = *result;
        }
        // Handle Cancelled, Superseded and SceneEnded as interruptions.
        return {};
    }
};
```

Declare ownership of Main and Cinematic before running this example. A same-slot blend returns an already Completed operation. Operations are copyable main-thread result views: `status()` reports Playing, Paused, Holding, Completed, Cancelled, Superseded or SceneEnded. `reached_target()` stays true after natural arrival, including while pausing or cancelling a Hold. Use `pause_motion`, `resume_motion`, `cancel_motion` (and the corresponding blend methods) with the typed operation. Holding is still controllable; Completed is finished. New motion or direct center/zoom changes supersede old motion; cuts and new blends supersede old blends. No subscriptions or general sequence player are provided.

Scene attaches the service before `on_runtime_attach` and `on_enter`. Detachment precedes exit callbacks, cancels remaining activity and marks live results SceneEnded; already terminal results remain unchanged. Old operations become stale after detachment/re-entry and cannot control a new scene. Retaining a result never retains a scene. Exit cleanup should clear local sequence state; Scene handles remaining camera activity. Constructors must not issue requests intended for a candidate scene: unscoped requests always target the currently attached scene.

Shake, snap and clear requests are queued in the scene runtime and forwarded in FIFO order on its next permitted update. Paused scenes defer them unless `advance_when_paused` is enabled. `clear_effects(slot)` is also queued; ordinary `cancel_activity()` cancels motion/blending and discards the Runtime's pending commands. Scene detachment synchronously clears active shake and pending Manager commands for owned slots, immediately removing shake offsets without another frame, even when paused. Same-scene Reuse preserves logical center, zoom, viewport, bounds, focus and follow configuration, but does not retain shake. Repeated cancellation and destruction of old runtimes do not perform bulk slot cleanup that could affect a new scene.

Let ordinary objects query cameras and request shake, and use one camera controller per scene to coordinate poses, following and cuts/blends. Later valid writes may still replace existing activity, reported by `Superseded`; the service provides no control lock or priority arbitration. With `ResolveEachFrame`, Scene supplies focus every frame; use Manual mode for persistent manual focus configuration.

Cancel scene-owned delayed tasks and subscriptions on detach. When cancellation is unavailable, check that the business object still exists and its activation generation still matches before submitting on the main thread. Old operation handles become stale, but a fresh unscoped request from an old callback still targets the currently attached scene. `is_available()` only indicates an active camera runtime; it does not establish the original scene's identity. All APIs are main-thread only; the service neither infers an object's owning scene nor advances cameras itself.

The showcase's `CameraCinematicController` demonstrates entering, touring, holding with shake, and returning to Main using only this service. Scene still owns actor freezing, automatic movement and UI.

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
