# World-Space Gameplay UI

This guide composes nameplates, world health bars, and bubbles with game objects. They follow world camera projection for nearby hints; screen menus/HUD remain ordinary UI. The complete type below owns a nameplate and submits text after joining an initialized scene. Actor sprite rendering is omitted.

`engine/gameplay/ui/` provides `elysia::gameplay::ui::WorldText`, `WorldBar`, and `SpeechBubble`. These ordinary object-owned components output world `RenderCommand`. Position, size, tail, and outline all project through the presentation camera. Order belongs to the submitting `GameObject`. Members release with the object and need no scene registration/unregistration.

## Text

```cpp
#include "engine/core/game_object.h"
#include "engine/gameplay/ui/world_text.h"

class NamedActor final : public elysia::core::GameObject
{
public:
    NamedActor() : GameObject(elysia::core::DepthLayer::Character)
    {
        set_size({32.0f, 48.0f});
        _nameplate.set_raw_text("Player");
        _nameplate.set_world_units_per_pixel(0.3f);
        _nameplate.set_color({220, 240, 255, 255});
    }

    void submit_render_commands(
        std::vector<elysia::core::RenderCommand>& commands) const override
    {
        // render_rect follows presentation; a bottom-center anchor places the name above the object.
        const elysia::core::Rect visual = render_rect();
        _nameplate.submit_render_commands(commands,
            {visual.center().x, visual.top() - 10.0f}, {0.5f, 1.0f});
    }

private:
    elysia::gameplay::ui::WorldText _nameplate;
};
```

The last parameter is the text's normalized anchor: default `{0,0}` places top-left at the position; `{0.5,1}` places bottom-center. `content_size()` reports world dimensions. Derive positions from `render_rect()` to follow physics presentation interpolation.

`set_text_key` queries current translations; `set_raw_text` takes UTF-8, or use `set_text_content(UiTextContent)`. Empty content emits no commands. Text reuses UI typography/source through `set_typography_role` / `set_font_source_override`.

`set_world_units_per_pixel` sets world units per source texture pixel, default 1, finite and positive; invalid input retains the old value. `set_max_width` also uses world units; default 0 disables wrapping and negative normalizes to 0. Width converts proportionally to at least 1 pixel; explicit line breaks remain. Zoom does not regenerate font textures. `set_visible` controls drawing without affecting measurement.

Default `Tint` uses white textures/command color modulation; `Baked` uses specified RGB textures. Both keep alpha in commands, so alpha-only changes do not rebuild textures.

Translated keys acquire shared cached textures on every submission, responding to language/cache changes. Raw-text textures are exclusively owned and rebuild for content, language, font generation, typography/source, wrap-pixel width, or baked RGB changes; position/`Tint` color reuse them. Empty/key content releases raw textures. Failed acquisition skips text and retries later.

Commands borrow textures and must execute before component mutation/destruction or localization cache clearing. Normal scene update/collection/execution satisfies this. Destroy components before their SDL renderer closes.

## Value Bars

`WorldBar` uses `set_range(min,max)`, `set_value(value)`, or `set_ratio(ratio)`. Default range `[0,1]`, value 0. Values/ratios clamp; inverted ranges/non-finite input are ignored; equal bounds produce empty bars. Four `WorldBarFillDirection` values fix edges. `WorldBarStyle` supplies background/fill/border colors and world border width; 0 disables borders.

`submit_render_commands(commands, world_rect)` draws background, fill, then border. Hidden components/empty rectangles draw nothing. Games provide health or other business values.

## Passive Bubbles

Configure `WorldText` via `SpeechBubble::text()`. Defaults are `DialogBody` and maximum width 160 world units. `SpeechBubbleStyle` supplies background, border, padding, tail size. `body_rect(tip)` combines text/padding above the tail tip. `submit_render_commands(commands, tip)` places the tip at the game position and draws background, tail, continuous border, then text. Empty/hidden text produces no bubble.

Bubbles clip normally offscreen, without automatic flipping/repositioning. Games own input, distance checks, dialogue advancement, duration, and branches.

## Combat Example and Validation

`BlockCombatActor` owns all three components. Nameplates use localized keys/precoloring; world health bars replace manual rectangles. Game logic shows a bubble for 1.5 seconds after hits; pause stops its owner timer. Anchors derive from actor `render_rect()`; screen HUD retains existing UI.

`world_ui_tests` verifies texture reuse/invalidation, tinting, value boundaries, bubble layout/drawing order, projection, and SDL software rendering. View actual visuals in physics/combat free play.

- [Screen UI](../ui/README.md#create-window), [camera](../systems/camera.md), [back to user guide](../README.md)
