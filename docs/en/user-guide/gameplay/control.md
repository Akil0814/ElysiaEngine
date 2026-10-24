# Controllers and Control Commands

This guide introduces actor control in session, creation, binding, and command-consumption order. Distinguish controller handles, binding operation results, and `target` lifetimes.

Controllers convert local input/custom intent to fixed-step `ControlCommand`. Games access the controller service through `ELYSIA_CONTROLLERS` to manage sessions, create controllers, and bind targets. Sessions here are game sessions, not network connections.

Include `engine/gameplay/control/controller_service.h` to use `ELYSIA_CONTROLLERS->method(...)`. This convenience macro is equivalent to `::elysia::gameplay::ControllerService::instance()` and only shortens service access. Sessions still require an explicit start, and callers must handle creation errors and binding operation results. `ControllerManager` handles internal scheduling; game code operates on controllers through the service.

Define actions/device maps through the [input guide](../systems/input.md), then integrate targets here. Repository wrappers live in `game/gameplay/control/` and `game/input/` under `example`; helpers/action IDs are not an engine-defined gameplay protocol.

## Integration Order

1. One game-flow owner calls `begin_session()` and handles `std::expected`; do not begin again in every subscene.
2. Enter `GameplayScene` and obtain valid `control_context()`.
3. Add targets implementing `ControlCommandReceiver` to that scene.
4. Create controllers, bind targets, and check final operation results.
5. Call `end_session()` when leaving the game session; controllers release and old handles must no longer be used.

## Creating Local Player Controllers

Place this helper in game code and call during active `GameplayScene` input/update. The session has started, `player` is registered, `map` matches devices, and `target` is in the scene and implements command reception.

```cpp
#include "engine/gameplay/control/controller_service.h"
#include "engine/gameplay/scene/gameplay_scene.h"
#include "engine/tools/logger.h"

#include <expected>
#include <optional>
#include <utility>

std::expected<elysia::gameplay::ControllerHandle,
              elysia::gameplay::ControllerError>
create_player_control(elysia::gameplay::GameplayScene& scene,
                      elysia::input::LocalPlayerId player,
                      elysia::input::InputActionMap map)
{
    using namespace elysia::gameplay;
    // Scope associates the scene context; creation arguments transfer map ownership to the controller.
    return ELYSIA_CONTROLLERS->create<LocalPlayerController>(
        ControllerCreateInfo{ControllerScope::Scene,
                             scene.control_context().token()},
        player, std::move(map));
}
```

Do not extract failed creation results. Save successful handles and call `bind_target(handle, scene.control_context(), target)`. See [action mapping](../systems/input.md) for devices/maps. Handles are not raw pointers and extend neither scene nor `target` lifetime.

## Confirm Binding Results

`bind_target`, `unbind_target`, and `replace_input_map` return `ControllerOperation`. Commit may defer across internal callback boundaries; function return does not mean binding completion.

```cpp
// Place in a void integration function; scene and target satisfy the prerequisites.
elysia::gameplay::ControllerService* service = ELYSIA_CONTROLLERS;
const std::expected<elysia::gameplay::ControllerHandle,
    elysia::gameplay::ControllerError> created =
    create_player_control(scene, player, std::move(map));
if (!created)
{
    ELYSIA_LOG_ERROR("control", "Controller creation failed: "
        << static_cast<int>(created.error()));
    return;
}
const elysia::gameplay::ControllerHandle handle = *created;
const elysia::gameplay::ControllerOperation operation =
    service->bind_target(handle, scene.control_context(), target);
if (operation.failed())
{
    const std::optional<elysia::gameplay::ControllerError> error = operation.error();
    if (error)
        ELYSIA_LOG_ERROR("control", "Target binding failed: " << static_cast<int>(*error));

    // The new controller is unused; log cleanup errors separately and retain the original binding failure.
    const std::expected<void, elysia::gameplay::ControllerError> removed =
        service->remove(handle);
    if (!removed)
        ELYSIA_LOG_ERROR("control", "Controller removal failed: "
            << static_cast<int>(removed.error()));
    return;
}
// Save handle; if operation.pending(), retain operation and check its terminal state later.
// Report binding success only after operation.succeeded().
```

`Pending` is not failure; never busy-wait on the main thread. On later `Failed`, handle `error()` such as invalid context, busy `target`/`player`, invalid `map`, or superseded request. Operation objects retain only results, not controllers/scenes/targets.

## Command Consumption and Cancellation

This `target` pairs with the [input guide](../systems/input.md)'s `game.move` `map`. Add to `GameplayScene` before `bind_target`. It has no rigid body and directly updates world position. Add drawing following [game objects](../scene/game-objects.md).

```cpp
#include "engine/core/game_object.h"
#include "engine/gameplay/control/control_command.h"

class ControlledObject final : public elysia::core::GameObject,
                               public elysia::gameplay::ControlCommandReceiver
{
public:
    ControlledObject() : GameObject(elysia::core::DepthLayer::Character)
    {
    }

    void on_control_command(const elysia::gameplay::ControlCommand& command,
                            double fixed_delta) override
    {
        // Commands arrive per fixed step; this nonphysical object moves directly without reapplying global scale.
        _movement = command.state.axis2d(elysia::input::InputActionId{"game.move"});
        set_position(position() + _movement * (100.0f * static_cast<float>(fixed_delta)));
    }

    void on_control_cancelled(elysia::gameplay::InputCancelReason) override
    {
        // Clear held intent on focus loss, pause, or unbinding; do not reuse the previous command.
        _movement = {};
    }

private:
    elysia::core::Vector2 _movement{};
};
```

Targets implement `on_control_command(const ControlCommand&, double fixed_delta)` and `on_control_cancelled(InputCancelReason)`. Commands contain continuous state, one-shot events, accumulated deltas, and tick/sequence. Delivery occurs on actual fixed steps. Events/deltas clear after delivery; do not expect one command per rendered frame.

Cancellation clears ongoing movement/charge intent so old input does not survive pause, source changes, or unbinding. Physical targets can record intent here and consume it in their physics phase; do not also advance the same motion in frame updates.

Cancellation exceptions propagate to scene boundaries after necessary cleanup. `Controller` cancellation failure neither prevents `target` notification nor other controller cleanup. Deferred configuration callback failure ends as `CallbackFailed` and unbinds the affected controller; handle scene failure rather than reuse old bindings. Session end invalidates all handles first; begin inside cancellation returns `SessionEnding`.

Custom AI and other sources derive `Controller` and submit `ActionInputResult` through protected `submit` from `produce_intent(tick, fixed_delta)`. No fake device events are needed; this does not imply network synchronization exists.

## Custom Controllers

This complete type demonstrates device-independent constant axes. Create it through `ELYSIA_CONTROLLERS->create<ConstantController>(...)`, passing an Axis1D action ID recognized by the target, then bind the target. No AI decisions or network synchronization are implemented.

```cpp
#include "engine/gameplay/control/controller.h"
#include <utility>

class ConstantController final : public elysia::gameplay::Controller
{
public:
    explicit ConstantController(elysia::input::InputActionId axis)
        : _axis(std::move(axis))
    {
    }

protected:
    void produce_intent(std::uint64_t, double) override
    {
        elysia::input::ActionInputResult intention;
        intention.frame.set(_axis, elysia::input::InputActionValueType::Axis1D,
                            {0.5f, 0.0f});
        // Submit to shared scheduling; do not call targets directly from controllers.
        submit(std::move(intention));
    }

private:
    elysia::input::InputActionId _axis;
};
```

Local controllers may override `on_mapped_input` to observe the sole active `map`, without parsing another device `map`. Fixed-step auxiliary behavior belongs in `produce_intent`. Tools must also clear retained business references when targets are removed.

## Scopes and Cleanup

Stop creating controllers when session creation fails; resolve repeated begin in game flow. `unbind_target(handle)` detaches targets; `remove(handle)` releases controllers. get returns temporary borrows; do not keep/use them after remove, context destruction, or end_session.

Scene scope binds to the creation context token; reset, scene destruction, or session end invalidates associated controllers. Session scope survives scenes, but exit still unbinds old targets and new scenes require explicit rebinding.

Leaving a cached scene alone does not reset its context. Avoid duplicating uncleared controllers on every `on_enter()`. Engine cancellation handles removal, exit, and pause; games still stop using invalid handles/borrowed `get()` pointers. Distinguish invalidation from other integration errors returned by `remove()`.

See [interface reference](../../reference/input/controllers.md) for the complete public contract.

## References

- [Public service](../../../../engine/gameplay/control/controller_service.h), [results/errors](../../../../engine/gameplay/control/controller_types.h)
- [Controller/target contracts](../../../../engine/gameplay/control/control_command.h)
- [GameplayScene](scene.md), [back to user guide](../README.md)
