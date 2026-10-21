# Game Objects and Rendering

This document explains object updates, drawing, and scene management. Creating an object, updating it each frame, and making it visible are three separate steps.

`GameObject` represents a world object and provides position, size, state, and drawing extension points. It does not automatically load images, respond to input, or participate in physics. Implement `Updatable` for ongoing behavior, integrate [physics](../systems/physics.md) for collision, and use [controllers](../gameplay/control.md) for actor input. UI uses the separate [UiElement hierarchy](../ui/README.md#create-window).

## Creating a Visible Moving Object

Place this type in a game header. In scene `on_enter()`, call `create_and_add_object<MovingBlock>()` and check the result. Retain existing instances or explicitly clean them on repeated entry; do not create duplicates.

The owning scene must configure `.camera = elysia::scene::CameraSceneConfig{}` in `SceneRuntimeFeatures` or `GameplaySceneFeatures` and center it near the object. Default scenes have no world camera; adding a world-command-submitting object alone triggers a render contract error. See the [camera example](../systems/camera.md#configuring-following) for complete integration.

```cpp
#include "engine/core/game_object.h"
#include "engine/core/interface/updatable.h"
#include "engine/core/render/render_command.h"
#include "engine/core/render/colors.h"

class MovingBlock final : public elysia::core::GameObject,
                          public elysia::core::Updatable
{
public:
    MovingBlock() : GameObject(elysia::core::DepthLayer::Item)
    {
        set_world_rect({40.0f, 40.0f, 32.0f, 32.0f});
    }

    void update(double delta) override
    {
        // delta already includes global scale; apply only this object's additional scale.
        elysia::core::Vector2 next_position = position();
        next_position.x += 50.0f * static_cast<float>(scaled_delta(delta));
        set_position(next_position);
    }

    void submit_render_commands(
        std::vector<elysia::core::RenderCommand>& commands) const override
    {
        elysia::core::RenderCommand command;
        command.type = elysia::core::RenderCommandType::FillRect;
        // Keep world coordinates; the scene projects commands after submission.
        command.command_rect = render_rect();
        command.color = elysia::core::colors::white;
        commands.push_back(command);
    }
};
```

Update interfaces register only when the object joins the scene; do not also call its `update()` manually. Object time scaling requires explicit `scaled_delta()`; the engine does not multiply automatically. This example has no rigid body and may directly change position.

`add_object` registers synchronously. Do not add/remove scene objects in object `update()`, UI presentation animations, rendering, queries, or physics simulation callbacks. Record spawn requests in objects and process them in scene `on_before_update` / `on_after_update`. Scene fixed hooks and input snapshot callbacks also permit synchronous addition. Registration callbacks permit nested addition, but not clearing. See the [phase table](scene.md#common-operations-and-extension-points).

## Position, Drawing, and Resources

`position()` is the world rectangle's top-left; `center()` its center. Modify with `set_position`, `set_center`, `set_size`, or `set_world_rect`. Logic uses `world_rect()`, drawing uses `render_rect()`, which may include physics interpolation. Teleport existing rigid bodies through physics APIs.

Texture drawing fills commands in `submit_render_commands()`: `type = Texture`, `texture = loaded texture`, `command_rect = render_rect()`. For cropping, set `use_src_rect = true` and texture-coordinate `src_rect`. `alpha` is 0–255; `rotation_degrees` is clockwise; `rotation_origin` is the normalized center inside the rectangle; `flip` uses `SpriteFlip`. Do not submit a texture command with a null texture.

Query textures through [ResourceService](../content/resources.md) and check null. Resources are borrowed and must not be freed by callers. Stop using old textures before reload. Animation objects produce the same command types through [animation interfaces](../systems/animation-and-effects.md). Do not load resources, advance time, or change rules while drawing.

Scenes project world commands through the current camera; do not first convert them manually to screen coordinates. World primitives also include rectangle outlines, circles, lines, and triangles. Prefer [debug drawing](../tools/debug-draw.md) for diagnostics. UI uses separate commands.

## State and Ordering

| Operation | Contract |
| --- | --- |
| `set_visible(false)` | Stop drawing this object, not updating it |
| `set_active(false)` | Exclude ordinary object updates; not complete physics deregistration or external service shutdown |
| `set_update_when_paused(true)` | Permit ordinary updates during scene pause; does not resume the physics world |
| `set_receive_input_when_paused(true)` | Part of input reception policy; adds no input interface automatically |
| `destroy()` | Mark destruction and stop using references; owner performs actual release |
| `reset()` | Restore base state; `GameObject` also restores time scale/display offset, while subclasses handle business fields |

Layers draw back to front: Background, Terrain, EffectBack, Item, Character, EffectFront, Foreground. Within one layer, larger constructor `order` draws later. Layer/`order` are fixed at construction; `Count` is a boundary marker. UI draws after the world. Drawing `order` is not input priority or physics collision layering.

`GameObject::reset()` does not restore spawn position, health, subscriptions, or animation state. Overrides should call the base and restore their own fields. Scenes own objects; pointers to other objects in members are borrowed and must clear on removal. Never query a raw pointer for liveness after destruction.

Object `reset()` and scene `on_reset()` are separate. Scene reset does not reset every object automatically; restore individually or clear/rebuild. Scenes reclaim marked objects at update end and after exit/reset. During `on_scene_object_removing` (`on_control_target_removing` for `GameplayScene`), objects are still valid; detach borrowed references there.

## References

- [Scenes and object management](scene.md), [core concepts](../core_concepts.md)
- [Drawing commands](../../../../engine/core/render/render_command.h), [GameObject](../../../../engine/core/game_object.h)
- [Back to user guide](../README.md)
