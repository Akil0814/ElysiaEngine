# Input and Action Mapping

This guide converts device operations to game actions and covers menu shortcuts, UI consumption, and local player ownership. Mapping examples are local configuration functions; see the controller guide for actors.

The application collects SDL input and scenes route it. Games use engine events/actions instead of a second SDL polling loop. Actor control normally uses [GameplayScene](../gameplay/scene.md) and [controllers](../gameplay/control.md); menu shortcuts can use scene hooks directly.

## Minimal Action Map

Place this helper in game input configuration. Move its successful result into `LocalPlayerController`; see [controllers](../gameplay/control.md) for creation, binding, and commands. Games define action names; these are not built-in protocol.

```cpp
#include "engine/input/action/input_action_map.h"

#include <optional>

std::optional<elysia::input::InputActionMap> make_move_map()
{
    using namespace elysia::input;
    InputActionMap map;
    const InputActionId move{"game.move"};
    // Combine four keys into one 2D action; do not publish partial maps on failure.
    if (!map.register_action({move, InputActionValueType::Axis2D}, {
        {move, Button2DInputBinding{
            RawInputControl::KeyA, RawInputControl::KeyD,
            RawInputControl::KeyW, RawInputControl::KeyS}}
    }))
        return std::nullopt;
    return map;
}
```

`register_action`, `add_binding`, and `replace_bindings` return bool; failed configuration is not success. `clear_bindings(action)` removes current bindings, `reset_defaults()` restores defaults, and `reset_state()` clears input state. After registration replace the whole map through service `replace_input_map`, not its read-only query result.

## Buttons, Axes, and Deltas

| Type | Binding and consumption |
| --- | --- |
| Button | `ButtonInputBinding`; `frame.is_pressed` queries held state; events distinguish Started/Canceled |
| Axis1D | Signed button scale or `AxisInputBinding`; `frame.axis1d(action)` |
| Axis2D | `Button2DInputBinding`, `Axis2DInputBinding`, or explicit X/Y components; `frame.axis2d(action)` |
| Delta | Descriptor `InputValueSemantics::Delta` with `PointerDeltaBinding`; read result `deltas` |

Gamepad buttons use Gamepad members of `RawInputControl`, sticks use `RawInputAxis`. SDL device indices are not `InputSourceId`. `State` axes apply dead zones, summation, and clamping. Delta selects MouseX/MouseY/WheelX/WheelY and preserves increments; do not multiply by delta or apply stick dead zones/[-1,1] clamps. One action cannot mix `State` and Delta; Button supports `State` only.

To add left stick to move above, call `map.add_binding({move, Axis2DInputBinding{RawInputAxis::GamepadLeftX, RawInputAxis::GamepadLeftY}})` before return and check bool. Buttons can use `ButtonInputBinding{RawInputControl::GamepadSouth}` or `ButtonInputBinding{RawInputControl::KeySpace}` with Button descriptors.

`InputSnapshot` contains initial/final states, ordered events, connection/removal lists, and focus loss per source. `find(source)` returns nullable borrows valid only while the original snapshot remains valid/unmodified. Press and release within one frame cannot be counted from final state alone; attacks/clicks needing order/count consume events.

Custom ordinary scenes can pass routed snapshots from `on_routed_input` to retained `InputActionMap::resolve()` for frame/events/`deltas`. Do not reconstruct maps every frame and lose state. `GameplayScene` already delegates this phase to controllers; do not parse/deliver again.

## Scene Shortcuts and UI Consumption

This complete scene handles only quit. Move `on_shortcuts()` into your scene to reuse it; `KeyEscape` is an example choice, not a universal engine shortcut.

```cpp
#include "engine/scene/scene.h"

class ShortcutScene final : public elysia::scene::Scene
{
protected:
    // This shortcut example has no business objects to create, detach, or reset.
    void on_enter(const elysia::scene::ScenePayload&) override
    {
    }

    void on_exit() override
    {
    }

    void on_reset() override
    {
    }

    void on_shortcuts(const elysia::input::RawInputFrame&,
        const std::vector<elysia::input::RawInputEvent>& events) override
    {
        using namespace elysia::input;
        for (const RawInputEvent& event : events)
        {
            if (event.type == RawInputEventType::ControlPressed &&
                event.control == RawInputControl::KeyEscape)
            {
                // Consume the original event to prevent later gameplay responding twice.
                consume_input(event);
                request_quit();
                return;
            }
        }
    }
};
```

UI goes first; consumption/capture affects later gameplay. `consume_input` uses current events without fabricated routing IDs. `set_shortcut_devices(InputCapture)` limits both shortcut frame state and events. `set_ui_gamepad` selects shared UI gamepad; `set_ui_interaction_mode` sets mode.

`set_all_gameplay_input_blocked(true)` temporarily blocks gameplay; explicitly restore false. Receivers added in UI callbacks join next dispatch; additions/destruction do not invalidate the current registration snapshot.

See [UI](../ui/README.md#create-window) for focus/modals/widgets. Never bypass routing to execute consumed attacks again from global raw input. Input converts window pointer coordinates to logical rendering; world aiming still requires [camera conversion](camera.md).

Scene UI enums belong to `elysia::scene`, for example `set_ui_interaction_mode(elysia::scene::UiInteractionMode::Navigation)`. Coordination lives under `engine/scene/input/`; base input/maps remain under `engine/input/`.

## Local Players and Device Ownership

Use `Scene::local_players()`. The primary defaults to full keyboard/mouse. `create_player()` adds players, `create_partition(name, keys)` partitions keyboards, `bind_keyboard` assigns partitions, and `bind_source` assigns mouse/gamepads. Check every expected result.

For repartitioning, copy `configuration()`, modify partitions/bindings, then atomically `apply_configuration(next)`. Do not first assign keys still owned by the primary to another player. Keys cannot conflict; each mouse/gamepad belongs to one player. Failure preserves configuration; maps must stay within player permissions.

`on_unassigned_input` receives unassigned gamepad events; use event source to join/assign and return true when handled. `transfer_source`, `replace_gamepad`, and `unbind_source` transfer/replace/detach. Do not treat keyboards as exclusive gamepads. Clear held intent on disconnect/focus loss rather than retain movement.

These local player/session capabilities provide no networking/state synchronization. Device configuration/maps do not automatically persist in user settings.

## Fixed Steps and Lifecycle

Controllers cache continuous state and ordered events/Delta for actual fixed steps. Zero steps retain one-shot input; consumption clears it, and later catch-up steps do not repeat `deltas`. Each controller holds at most 1024 pending events; overflow cancels the entire pending input and logs diagnostics.

Pause, unbinding, map replacement, and device changes trigger cancellation or release/neutral gates. Target cancellation clears ongoing actions. Map replacement may return Pending; confirm final [operation results](../gameplay/control.md).

## Game Schemes and Keyboard Partitions

Game `make_gameplay_input_map(InputScheme)` uses KeyboardScheme Wasd, Arrows, or None plus mouse/gamepad flags. Controllers still have one active map. WASD/arrows do not combine by default; the arrow example defines movement only, without forced attack/pickup bindings.

`keyboard_keys(scheme)` supplies required keys. After partition creation, atomically assign players before creating/binding maps. Replacing maps/modifying partitions validates permissions; maps may retain bindings for absent devices. Pickup, doors, and map interaction are gameplay actions; UI shows hints rather than directly executing world behavior from hint controls.

See [action mapping reference](../../reference/input/action-mapping.md) for values/replacement contracts.

## References

- [Action definitions](../../../../engine/input/action/input_action_types.h), [player configuration](../../../../engine/input/local_player_registry.h)
- [Scenes](../scene/scene.md), [controllers](../gameplay/control.md), [back to user guide](../README.md)
