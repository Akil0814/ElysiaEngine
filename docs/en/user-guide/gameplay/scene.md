# GameplayScene Guide

This guide helps choose `GameplayScene` and explains its additions over ordinary scenes. Understand lifetime before integrating control/collision so framework scheduling is not duplicated.

`GameplayScene` suits levels needing actor control or gameplay collision. Ordinary `Scene` provides objects, input routing, and UI, with optional fixed steps/cameras/physics. `GameplayScene` defaults to fixed steps and control context; physics, collision, and camera remain explicit. Menus/showcases need not become `GameplayScene` merely to use physics.

See [scene system](../scene/scene.md) for ordinary lifetime, objects, and routing. This page covers additional capabilities/constraints.

## Minimal Scene

Place this type in a scene header. It demonstrates valid hooks without actors/resources/route registration; see [initialization](../game-initialization.md) for registration.

```cpp
#include "engine/gameplay/scene/gameplay_scene.h"

class BattleScene final : public elysia::gameplay::GameplayScene
{
public:
    // This only shows fixed hooks, without actors or external listeners.
    void on_enter(const elysia::scene::ScenePayload&) override
    {
    }

    void on_exit() override
    {
    }

    void on_reset() override
    {
    }

protected:
    void on_game_fixed_update(std::uint64_t tick, double delta) override
    {
        (void)tick;
        (void)delta;
        // Put fixed-step rules that run after control delivery here.
    }
};
```

Empty entry/exit/reset suits only this stateless example. Real scenes create/restore objects and clean their listeners/callbacks/associations. `on_reset()` does not automatically clear all objects.

## Runtime and Extension Points

Before entry, the manager activates control context and, only if enabled, binds gameplay collision runtime to the service. Exit unbinds these before `on_exit()`, so do not rely on the global collision service still pointing here. Clean scene bindings through protected `collision_runtime()`.

`Scene`-owned `control_context()` supplies the token for scene-scoped creation. Do not create/delete contexts or fabricate tokens yourself.

| Extension point | Purpose |
| --- | --- |
| `on_enter`, `on_exit`, `on_reset` | Game entry, exit, reset |
| `on_game_fixed_update` | Rules after controller fixed-step delivery |
| `on_control_target_removing` | Business references on removal, after controller cleanup |
| `on_before_update` / `on_after_update` | Frame logic before objects / after physics/camera; framework schedules base work and reclaims after `on_after_update` |

Do not override `on_routed_input`, `on_fixed_update`, `on_pause_changed`, `on_scene_object_removing`, or `on_runtime_attach` / `on_runtime_detach` / `on_runtime_reset`; these are `final`. Fixed steps advance controllers, invoke `on_game_fixed_update`, then run physics participants/simulation.

`Scene` fixed hooks may add synchronously. Once physics simulation begins, participant/collision callbacks cannot add/remove objects; buffer until `on_after_update`.

## Lifecycle and Pause

- Pause cancels control input/stops scene physics fixed steps, not the whole application/all UI.
- Exit unbinds targets without destroying cached scenes. `Scene`-scoped controllers release on context reset/destruction; restore bindings on reentry instead of unconditionally creating duplicates.
- Reset resets control context before `on_reset()` without clearing business objects. Recreate constructs a candidate, exits current, destroys target cache, then activates it. Session controllers still depend on game session; retained handles do not mean valid targets.
- Ordinary objects reclaim after update and exit/reset callbacks. `destroy()` in `on_exit()` is not immediate but reclaims within that exit.
- Failed `on_enter` receives no `on_exit`. Use RAII/local rollback for external subscriptions/callbacks. See [scene lifetime](../scene/scene.md) for order/mutation phases.

Begin sessions before controller creation/binding; register physical colliders before actor association. See [controllers](control.md) and [collision](collision.md). Treat failed service binding, invalid tokens, or bad routes as integration errors rather than bypassing checks.

## Menu and Gameplay Input

Menus/loaders use ordinary `Scene` UI/shortcuts without controllers. `GameplayScene` defaults to Pointer; switch to Navigation for menus and restore on closing. Independent `set_ui_gamepad` does not change player ownership.

`set_all_gameplay_input_blocked` blocks local gameplay only; `pause` stops world controllers/cancels caches. Recovery requires release/neutral conditions. See [input](../systems/input.md) for shortcuts/unassigned joining.

## Local Two-Player Integration

One flow starts the session, assigns each player partitions/gamepads, builds maps, and explicitly creates/binds controllers. Shared keyboards do not mean every player receives the full keyboard; map/partition permissions must agree.

Repository Local multiplayer retains Session handles. WASD defaults to player 1, arrows to player 2, mouse to player 1. Start on an unbound gamepad joins player 2. Shared menus configure keyboard schemes, mouse/gamepad ownership, and independent UI gamepad.

Entry binds blue/red actors; exit unbinds while retaining session controllers; reentry rebinds. It saves external keyboard/mouse configuration on entry and restores on exit; gamepad ownership persists. Returning to main menu ends the session and invalidates old handles.

The example shares a camera with two actors, without split-screen, networking, or personal focus trees. Registries/controller management are not limited to two players. Check all configuration results; accepted Pending is not binding success. See [controller guide](control.md).

## References

- [Public definition](../../../../engine/gameplay/scene/gameplay_scene.h) and [implementation](../../../../engine/gameplay/scene/gameplay_scene.cpp)
- [Object lifetime](../core_concepts.md)
- [Back to user guide](../README.md)
- [Attack/damage example](damage.md)
