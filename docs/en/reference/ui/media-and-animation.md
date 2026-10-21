# Images, Animation, and Presentation Effects

UI animation advances presentation, not layout or required game rules. Ordinary interfaces use components; custom elements compose state machines directly.

See [UI guide](../../user-guide/ui/README.md#create-window) and [reference index](README.md#browse-by-class).

<a id="media-controls"></a>
## Media Controls: UiImage and UiAnimation

Images show existing textures; animations `play` registered resources. Both present UI without game rules.

Headers: [ui_image.h](../../../../engine/ui/widgets/image/ui_image.h), [ui_animation.h](../../../../engine/ui/widgets/image/ui_animation.h).

<a id="media-controls-uiimage"></a>
### UiImage

Constructors borrow `SDL_Texture*` with rectangle/position/center geometry. `set_texture` replaces borrows; callers own lifetime. `set_source_rect` crops texture coordinates; `clear_source_rect` restores full texture.

<a id="media-controls-uianimation"></a>
### UiAnimation

`set_animation_key` binds game keys; `set_engine_animation(BuiltinAnimationId)` built-ins. Success returns `true`/restarts/plays. Failure returns `false`/clears animation/key rather than retaining old images. Key constructors cannot return results; explicitly `bind` when checking failure. See [implementation](../../../../engine/ui/widgets/image/ui_animation.cpp).

`set_loop` overrides definitions and persists across rebinding. `play` restarts, `pause`/`resume` continue, reset resets element/current animation. Unbound `is_finished`=`false`/is_paused=`true` and emits no commands. `animation_key`/`is_looping` query settings.

See [opacity variants](#opacity-variants), which inherit texture/source APIs.

<a id="opacity-variants"></a>
## Opacity Animation Components

These compose `opacity` state machines with text/images for fades/blinks/pulses. UI updates advance playback.

Headers: [ui_fade_label.h](../../../../engine/ui/widgets/label/ui_fade_label.h), [ui_blink_label.h](../../../../engine/ui/widgets/label/ui_blink_label.h), [ui_pulse_label.h](../../../../engine/ui/widgets/label/ui_pulse_label.h), [ui_fade_image.h](../../../../engine/ui/widgets/image/ui_fade_image.h), [ui_blink_image.h](../../../../engine/ui/widgets/image/ui_blink_image.h), [ui_pulse_image.h](../../../../engine/ui/widgets/image/ui_pulse_image.h).

All three Label/Image pairs inherit full `UiLabel`/`UiImage` APIs and add:

1. `configure_playback`(mode,...) for `Fade`/`Blink`/`Pulse`;
2. Optional `set_on_end(callback)` for one completion;
3. `play` resets/starts;
4. Containers call `update(delta)`; components apply computed `opacity`.

`Fade` suits single transitions, `Blink` visible/hidden durations, `Pulse` min/max `opacity`. `Blink`/`Pulse` optional counts default `nullopt` (unbounded); zero completes immediately in `play`. Register callbacks before starting.

Completed lifetimes differ: `UiFadeLabel`/Image first mark destroy then notify for container cleanup; `Blink`/`Pulse` notify but retain. Replaying resets notification without undoing `Fade` destruction; finished Fades cannot be reused this way.

Label `play` is `noexcept`; synchronous completion exceptions must not escape. Store errors for host handling. Captures must honor host/control lifetime.

<a id="animation-primitives"></a>
## UI Animation State Machines

For custom elements, machines only calculate `translation`/`opacity` without drawing/automatic advancement. Prefer existing animated Label/Image for ordinary UI.

Headers: [ui_translation_animation_player.h](../../../../engine/ui/effects/ui_translation_animation_player.h), [ui_opacity_blink_core.h](../../../../engine/ui/effects/ui_opacity_blink_core.h), [ui_opacity_fade_core.h](../../../../engine/ui/effects/ui_opacity_fade_core.h), [ui_opacity_pulse_core.h](../../../../engine/ui/effects/ui_opacity_pulse_core.h).

<a id="animation-primitives-位移"></a>
### Translation

`UiTranslationAnimationPlayer` `bind`(name,animation) registers, `play(name)` starts, `stop` stops, `update(delta)` advances. `remove`/`clear` unregister; `translation` reports presentation offset for `UiElement` without changing layout.

Empty `bind` does nothing; same names replace definitions. Missing `play` returns `false`/preserves playback; success `true`, nonpositive durations jump to end. update returns visible change. `stop` preserves offset; removing active definitions stops too. `clear` additionally zeros offset. Completion makes is_playing `false` but active_name remains until `stop`/`clear`.

<a id="animation-primitives-透明度"></a>
### Opacity

Three machines `configure_playback`/`play`/update/`opacity`/`is_finished`. Owning elements apply results; see [components](#opacity-variants).
