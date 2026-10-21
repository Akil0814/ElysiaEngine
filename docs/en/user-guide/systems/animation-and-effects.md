# Animation and Effects

This document explains playback of loaded animations and drawing effects from registered definitions. Keep resource configuration, playback state, and game rules separate; do not advance animations or grant effects during rendering.

Declare atlases, animations, and effects in [resources](../content/resources.md), then wait for loading success. Games need not initialize/close animation or effect services. Choose by presentation ownership:

| Need | Entry point and ownership |
| --- | --- |
| Object animation | `AnimationService::create_animation()` returns a caller-owned, caller-updated player |
| World hit effects/floating numbers | `EffectService` synchronously adds effect objects to the active scene for updates/reclamation |
| Flash, blackout, screen animation | `EffectService` returns screen-effect handles; effects clear on owning scene exit |

## Playing Object Animation

Place this class in a game header and create it on scene entry after resources load. `test.animation` matches the manifest below; the project supplies the image. Enable a camera and aim at the object; see [camera integration](camera.md#configuring-following).

```cpp
#include "engine/animation/animation_service.h"
#include "engine/core/game_object.h"
#include "engine/core/interface/updatable.h"
#include "engine/tools/logger.h"

class AnimatedItem final : public elysia::core::GameObject,
                           public elysia::core::Updatable
{
public:
    AnimatedItem() : GameObject(elysia::core::DepthLayer::Item)
    {
        set_world_rect({100, 100, 64, 64});
        // The object owns its animation player; the resource system owns the borrowed atlas.
        _animation = ELYSIA_ANIMATIONS->create_animation("test.animation");
        if (!_animation)
        {
            ELYSIA_LOG_ERROR("animation", "Missing test.animation");
            destroy();
        }
    }

    void update(double delta) override
    {
        // Advance playback only during updates; rendering reads the current frame.
        if (_animation)
            _animation->update(scaled_delta(delta));
    }

    void submit_render_commands(
        std::vector<elysia::core::RenderCommand>& out) const override
    {
        if (!_animation)
            return;
        elysia::core::RenderCommand command;
        // Skip when no valid command can be generated; never submit uninitialized commands.
        if (_animation->build_render_command(render_rect(), 0.0,
                elysia::core::SpriteFlip::None, command))
            out.push_back(command);
    }

private:
    std::unique_ptr<elysia::animation::Animation> _animation;
};
```

The caller owns the player, which borrows `Atlas` and frame textures. Destroy/deactivate players using old resources before reload; pausing does not keep borrowed resources valid.

`set_loop` sets looping, `set_interval_seconds` frame interval, `pause`/`resume` `pause`/continue, and reset restarts at frame one with fresh timing. Non-looping completion sets is_finished and holds the last frame; `resume` does not replace reset. `set_on_finished` takes a no-argument callback; do not free the currently updating player inside it.

`current_frame` and service `find_definition` return nullable borrows. `append_render_commands` can apply a color overlay. Do not assume commands were generated when false is returned; overlays need the frame's coverage mask. Usually use build_render_command above.

## Requesting Scene Effects

Call this helper in a safe active-scene phase, such as `on_enter`, `on_before_update`, or `on_after_update` processing accumulated presentation requests. Effects synchronously use `add_object`; do not directly spawn from object updates, UI presentation animation, rendering, queries, physics collision listeners, or object-effect completion callbacks. Save value requests first. `on_after_update` follows physics/camera and precedes reclamation. See [scene phase table](../scene/scene.md#common-operations-and-extension-points).

```cpp
#include "engine/effects/effect_service.h"
#include "engine/tools/logger.h"

void show_hit(elysia::core::Vector2 position)
{
    elysia::effects::AnimationEffectSpawnRequest request;
    request.effect_key = "effect.test";
    request.position = position;
    request.anchor = elysia::effects::EffectAnchor::Center;
    if (!ELYSIA_EFFECTS->request_animation_effect(request))
        ELYSIA_LOG_WARN("effects", "Hit effect was not created.");

    elysia::effects::FloatingNumberEffectSpawnRequest number;
    number.text = "120";
    number.position = position;
    number.target_height = 20.0f;
    number.lifetime_seconds = 0.6;
    if (!ELYSIA_EFFECTS->request_floating_number_effect(number))
        ELYSIA_LOG_WARN("effects", "Damage number was not created.");
}
```

`Animation` effect position is the chosen anchor's world position. Override size, `angle_degrees`, `flip`, or start_delay_seconds as needed. Missing active scene/definition or creation failure returns false. Creation is immediate; true does not mean playback completed.

Non-looping completion calls on_finished and marks destruction; looping never ends naturally. No cancellation handle is returned. Prefer owning `Animation` in your own object for managed long-lived presentation. Exit need not invoke completion; cached Reuse scenes may retain effects.

on_started, on_finished, and scheduled_callbacks receive `AnimationEffect`&; scheduled delay_seconds is relative to playback time. Captures must outlive actual execution; completion callbacks cannot be the only external cleanup path.

Floating numbers need no animation manifest but require valid fonts/rendering. text supports `0–9`, `-`, `.`, `/`, `%`, not plus signs or ordinary text; use UI for text. Position, height, and times must be valid; lifetime_seconds/target_height positive, time_scale nonnegative. effects combine linear/arc motion, scale, and fade with 0–1 lifetime time_range. Without these, lifetime still ends in destruction. on_finished receives `FloatingNumberEffect`&.

## Full-Screen Effects

`EffectService` supports solid color, static images, and animated screen effects. They use logical game viewport screen coordinates, unaffected by camera movement/zoom, excluding window letterboxing. All belong to the active scene and are removed on exit (including cached reuse), fault recovery, shutdown, or content cleanup, invalidating handles. They cannot persist across scenes.

```cpp
#include "engine/effects/effect_service.h"

// White flash: display immediately, then fade out over 0.2 seconds.
std::optional<elysia::effects::ScreenEffectHandle> flash_white()
{
    elysia::effects::ScreenColorEffectRequest request;
    request.color = {255, 255, 255};
    request.playback.hold_seconds = 0.0;
    request.playback.fade_out_seconds = 0.2;
    return ELYSIA_EFFECTS->request_screen_color_effect(request);
}

// The image must already be loaded through project texture configuration.
std::optional<elysia::effects::ScreenEffectHandle> show_screen_image()
{
    elysia::effects::ScreenImageEffectRequest request;
    request.texture_key = "screen.blood"; // Project-provided texture key
    request.fit = elysia::effects::ScreenEffectFit::Cover;
    request.playback.fade_in_seconds = 0.1;
    request.playback.hold_seconds = 0.3;
    request.playback.fade_out_seconds = 0.4;
    request.playback.layer = elysia::effects::ScreenEffectLayer::BeforeUi;
    return ELYSIA_EFFECTS->request_screen_image_effect(request);
}
```

Solid/image requests share `ScreenEffectPlayback`. Defaults: target opacity 1, fade-in/out 0 seconds, hold 0.6 seconds, end `Timed`, layer `AfterUi`, clock `Unscaled`. Opacity linearly fades from 0 to target, holds, then fades out. Zero fade-in starts at target opacity. Color alpha, image alpha, and playback opacity multiply.

`Timed` hold begins after fade-in; `Manual` holds until explicitly stopped. `stop_screen_effect(handle)` fades from current opacity using the request's fade-out; zero removes immediately. Repeated stop on an already fading effect returns false without restarting. `cancel_screen_effect(handle)` removes immediately. `is_screen_effect_active(handle)` reports existence, not fade-in completion. Invalid handles safely return false and cannot affect new effects in reused slots.

`Unscaled` uses raw frame time, independent of scaling/`pause`. `Scene` uses scaled time and scene `pause`. Existing effects advance before scene updates each frame; effects created by current input, entry, or update begin timing next frame. Requests invoke no user callbacks and may be created/cancelled in object updates.

`BeforeUi` draws after world/debug and before UI; `AfterUi` covers UI. Development panels follow both. Within a layer, creation order determines stacking. Image `Stretch` fills the viewport; `Cover` preserves aspect, centers, and crops; `Contain` preserves aspect and fully displays, leaving uncovered regions transparent.

Creation returns optional handles. No active scene, missing texture, negative/non-finite times, opacity outside 0–1, or all-zero `Timed` durations return empty and log. Images borrow loaded textures; clear effects through existing content cleanup before reload. Directly freeing referenced textures is unsupported. Shader postprocessing is unsupported.

### Screen Animation and Shared Layout

`request_screen_animation_effect` shares world effects' `effect_key`, loaded animation, `Atlas`, and textures, with independent players per request. Defaults: `loop=false`, `fit=Contain`, `end=AnimationFinished`, hold 0. Resource loop settings do not override screen requests. Angle follows the definition unless overridden with `angle_degrees`; `flip` defaults None.

This fragment belongs in an active scene function including `effect_service.h`. Requests are independent; check each returned optional handle. Image keys must already be loaded.

```cpp
// One centered presentation: show the complete animation, then fade out.
elysia::effects::ScreenAnimationEffectRequest single;
single.effect_key = "effect.test";
single.playback.fade_in_seconds = 0.2;
single.playback.fade_out_seconds = 0.4;
const std::optional<elysia::effects::ScreenEffectHandle> single_handle =
    ELYSIA_EFFECTS->request_screen_animation_effect(single);

// Looping full-screen mask: wait for caller stop/cancel.
elysia::effects::ScreenAnimationEffectRequest overlay = single;
overlay.loop = true;
overlay.fit = elysia::effects::ScreenEffectFit::Cover;
overlay.playback.end = elysia::effects::ScreenEffectEnd::Manual;
const std::optional<elysia::effects::ScreenEffectHandle> overlay_handle =
    ELYSIA_EFFECTS->request_screen_animation_effect(overlay);

// Bottom-right animation: natural size, inset by 24 logical pixels.
elysia::effects::ScreenAnimationEffectRequest corner = single;
corner.fit = elysia::effects::ScreenEffectFit::Natural;
corner.placement.anchor = elysia::effects::EffectAnchor::BottomRight;
corner.placement.offset = {-24, -24};
corner.placement.scale = {0.5f, 0.5f};
const std::optional<elysia::effects::ScreenEffectHandle> corner_handle =
    ELYSIA_EFFECTS->request_screen_animation_effect(corner);

// Static images use the same placement parameters.
elysia::effects::ScreenImageEffectRequest image;
image.texture_key = "demo.screen_effect";
image.fit = corner.fit;
image.placement = corner.placement;
const std::optional<elysia::effects::ScreenEffectHandle> image_handle =
    ELYSIA_EFFECTS->request_screen_image_effect(image);
```

`Animation` and fade-in advance together next frame. `AnimationFinished` waits for both before fading out and ignores hold time; looping cannot use it. Completed non-looping animation in `Timed`/`Manual` holds its last frame. `Animation` continues during stop fade-out. Every frame lasts one full interval; one playback lasts frame count / FPS, and remaining large delta advances fade-out.

Image/animation share `ScreenEffectPlacement`, default centered, offset `{0,0}`, scale `{1,1}`. Compute fitted size, apply scale, align matching content/viewport anchors, then add offset. Nine anchors control placement and `Cover` crop direction. All modes clip to the logical viewport. `Natural` uses original image size or effect default size for animation, otherwise first-frame size. Other modes fit the first-frame aspect ratio. `Animation` size does not change across frames. Rotation is about rectangle center, independent of placement anchor.

Offsets/angles must be finite, scales finite and positive. Missing effect/animation/valid frames and invalid end-policy combinations reject creation. Solid/image requests do not accept `AnimationFinished`.

### Blackout Transitions Across Two Scenes

The old scene requests fade-to-black and accumulates the same clock, requesting a route switch after fade-in. `Manual` holds black before switching. Do not start transition timing if creation fails.

```cpp
std::optional<elysia::effects::ScreenEffectHandle> begin_exit_blackout()
{
    elysia::effects::ScreenColorEffectRequest request;
    request.color = {0, 0, 0};
    request.playback.fade_in_seconds = 0.4;
    request.playback.end = elysia::effects::ScreenEffectEnd::Manual;
    return ELYSIA_EFFECTS->request_screen_color_effect(request);
}

// Call in the new scene's on_enter to create a black mask before first rendering.
std::optional<elysia::effects::ScreenEffectHandle> begin_enter_blackout()
{
    elysia::effects::ScreenColorEffectRequest request;
    request.color = {0, 0, 0};
    request.playback.hold_seconds = 0.0;
    request.playback.fade_out_seconds = 0.4;
    return ELYSIA_EFFECTS->request_screen_color_effect(request);
}
```

With the default clock, accumulate `ELYSIA_TIME->raw_delta()` starting next update and request switching at 0.4 seconds. Exit clears the old mask; new scene entry creates a new mask starting black and fading out. Its first frame stays black without a cross-scene handle. Games own routing and transition timing.

### Manual In-Game Testing

Main menu → module showcase → effects. The screen-effect area below the title supports buttons/shortcuts; floating-number controls remain below:

| Key | Effect |
|---|---|
| F1 | Flash white and fade out |
| F2 / F3 | Fade black then restore / hold black |
| F4 / F5 | Fade from current opacity / cancel immediately |
| F6 / F7 / F8 | Image `Stretch` / `Cover` / `Contain` |
| F9 | Switch before/after UI layer; clear current preview then choose again |

The panel reports playing, completed, or rejected status in the project's five languages. Each choice replaces the previous preview. Shortcuts work when black covers buttons. Esc/Back clears an active effect first and returns only without one. Loaded `demo.screen_effect` has a 3:2 grid, corner markers, and rings for inspecting stretch, crop, aspect, and transparency.

`Animation` controls include play, loop, end policy, fit, anchor, offset, scale, and image playback using current layout. Changes apply to the next playback; F4/F5 also control animation. Enabling loop changes `AnimationFinished` to `Manual`; choosing `AnimationFinished` disables looping. Loaded `effect.test` also creates a world effect with the same key for comparison.

These are manual observation steps, not automated results. F2 also toggles enabled development overlays; use page buttons for shortcut conflicts. See [effects showcase](../../../../game/showcase/effects/effects_showcase_scene.cpp).

## Resource Configuration

Prepare images/animation declarations before definitions reference those animations. Registry `required.animations` and `required.effects` point to the two manifests.

### Adding a Core Animation

Place a horizontal 14-frame image at `assets/textures/test/frame_group.png`, then declare:

```json
{
  "animations": [
    {
      "key": "test.animation",
      "path": "test/frame_group.png",
      "frame_count": 14,
      "fps": 10,
      "loop": false,
      "horizontal_strip": true
    }
  ]
}
```

Use one row of equal-width frames, with width divisible by frame count. After loading, create `test.animation` and let its owner update it. For frame directories/entity animation, follow [animation format reference](../../reference/resources/animation-and-effects.md) for prefixes, layout, and configuration.

### Declaring an Animation as an Effect

Map an animation key to an effect key in the effect manifest:

```json
{
  "effects": [
    {
      "key": "effect.test",
      "animation_key": "test.animation",
      "default_width": 128,
      "default_height": 128,
      "default_angle_degrees": 0
    }
  ]
}
```

After loading, request `effect.test` through the service. Definitions do not replace animation loading; nonexistent references fail. See [effect format](../../reference/resources/animation-and-effects.md#effects) for module effects, segment numbering, and sizes.

For configuration failures, inspect registry manifest references, image dimensions, complete frame sets, animation keys, and duplicate resources of the same type. Filesystem segment numbers differ from runtime keys; see [frame sources and segments](../../reference/resources/animation-and-effects.md#frame-sources-and-segments).

## Common Misuse and References

Created animations do not update automatically; the owning `Updatable` drives them once per frame. Effects already are scene objects and must not be manually updated. Do not advance animation during drawing, retain expired `Atlas`, or make required game outcomes depend on animation completion callbacks.

- [Animation API](../../../../engine/animation/animation.h), [effect parameters](../../../../engine/effects/effect_types.h)
- [Scene lifetime](../scene/scene.md), [time](time-and-timers.md), [back to user guide](../README.md)
