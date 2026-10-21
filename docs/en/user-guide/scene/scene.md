# Scene System

This document explains scene creation/registration, extension points, reentry, and failure cleanup for game scene authors. Complete types demonstrate valid interfaces; local call fragments belong in the phases specified alongside them.

`Scene` organizes game objects and UI and coordinates input, frame updates, fixed-step physics, cameras, and drawing. Menus, showcase pages, and simple levels can derive directly from it; use [GameplayScene](../gameplay/scene.md) for controllers and gameplay collision. Read [core concepts](../core_concepts.md) first, then organize concrete code here.

## Creating and Entering Scenes

Place this type in a game scene header. It contains one object with no update or drawing commands to illustrate reuse/reset; see [game objects and rendering](game-objects.md) to make objects visible.

```cpp
#include "engine/scene/scene.h"
#include "engine/scene/scene_manager.h"
#include "engine/scene/routing/scene_payload.h"

struct RoomPayload
{
    int score = 0;
};

class RoomScene final : public elysia::scene::Scene
{
public:
    void on_enter(const elysia::scene::ScenePayload& payload) override
    {
        if (const RoomPayload* data = elysia::scene::try_scene_payload<RoomPayload>(payload))
            _score = data->score;
        // This accepts empty Payload; scenes requiring parameters must reject wrong types.
        if (!_object)
            _object = create_and_add_object<elysia::core::GameObject>(
                elysia::core::DepthLayer::Item);
        if (!_object)
            return; // Creation failed; skip dependent logic.
        _object->set_position({100.0f, 100.0f});
        resume();
    }

    void on_exit() override
    {
        // Retain objects for reentry here; detach external subscriptions.
    }

    void on_reset() override
    {
        // Framework reset does not reset business objects; restore scene data explicitly.
        _score = 0;
        if (_object)
            _object->reset();
        resume();
    }

protected:
    void on_scene_object_removing(elysia::core::SceneObject& object) override
    {
        // Objects remain valid during removal notification; clear borrows before they dangle.
        if (&object == _object)
            _object = nullptr;
    }

private:
    elysia::core::GameObject* _object = nullptr; // Scene-owned; borrowed here.
    int _score = 0;
};

// Called by IGameModule::register_scenes; keep project identifiers unique.
inline void register_room(elysia::scene::SceneManager& scenes)
{
    scenes.register_game_scene<RoomScene>(1);
}
```

Game SceneKey values use `1..999`; do not reuse engine-reserved identifiers or register duplicates. Constructor arguments are retained for reconstruction and must meet registration's copyability requirements. For borrowed dependencies using `std::ref`/`std::cref`, guarantee lifetime yourself. See [game initialization](../game-initialization.md) for a complete application entry point.

## Common Operations and Extension Points

| Interface | Usage and rules |
| --- | --- |
| `create_and_add_object<T>(...)` | Create a `GameObject` or `UiElement` subclass and transfer it to the scene; returns a borrowed pointer, possibly null on failure |
| `add_object(std::unique_ptr<T>)` | Transfer existing ownership; do not release it yourself afterward |
| `pause()` / `resume()` | Change pause state; camera playback freezes by default, overridable with `advance_when_paused` |
| `on_before_update(double)` / `on_after_update(double)` | Business logic before ordinary updates / after physics and camera advancement; automatically scheduled, do not call base update entry points |
| `on_reset()` | Business reset for `Reset` routes; engine has reset input, pause, fixed steps, and camera runtime |
| `on_fixed_update(tick, delta)` | Ordinary scene fixed-step rules; runs only on actual simulation steps |
| `on_shortcuts(frame, events)` | Shortcuts after UI; consume handled events with `consume_input(event)` |
| `on_unassigned_input(event)` | Unassigned-device input, such as joining; return whether handled |
| `on_routed_input(snapshot)` | Input processed by scene routing; `GameplayScene` owns this extension point |
| `on_pause_changed(bool)` | Ordinary scene pause response; `GameplayScene` has its own final implementation |
| `on_scene_object_registered` / `on_scene_object_removing` | Synchronize business associations; preserve base registration behavior when overriding; do not destroy an executing listener during removal callbacks |
| `has_camera()` / `camera()` | Query camera capability/access final presentation camera; forced access without one throws a contract exception |
| `try_camera_runtime()` / `camera_runtime()` | Query/access camera runtime in derived scenes, following physics capability access rules |
| `resolve_camera_focus(slot)` | Supply follow targets for each owned slot without storing camera internals |
| `camera_runtime().cut_to()` / `blend_to()` | Select or smoothly switch the final presentation slot |
| `camera_runtime().move_to()` / `play_path()` | Play a single move or multi-node camera pose path |

See [input](../systems/input.md), [time](../systems/time-and-timers.md) for fixed steps/pause, [physics](../systems/physics.md), and [camera](../systems/camera.md) for coordinates. The scene rendering entry point is private and non-virtual; submit commands through objects rather than overriding `on_render()`.

Like physics, cameras are optional. Pure UI scenes need no camera; scenes submitting world commands or world `DebugDraw` must explicitly provide `CameraSceneConfig` in `SceneRuntimeFeatures` or `GameplaySceneFeatures`. Default `GameplayScene` enables fixed stepping and control only.

`add_object` synchronously transfers ownership and registers interfaces; it is not a deferred command. Prohibited phases throw contract exceptions before changing containers. Buffer spawn requests until `on_before_update` or `on_after_update`. `clear_scene_objects()` also requires a safe phase.

| Current phase | Synchronous addition | Clearing objects |
| --- | --- | --- |
| `on_enter`, `on_reset`, `on_exit` | Allowed, though exit normally only cleans up | Allowed |
| `on_before_update` / `on_after_update` | Allowed | Allowed |
| `Scene` `on_fixed_update` / `on_game_fixed_update`, before physics simulation | Allowed | Allowed |
| Snapshot-based input callbacks | Allowed | Allowed; dispatch skips removed objects |
| Object `update()`, UI presentation animation traversal | Prohibited | Prohibited |
| World/UI command submission, object-query visitor or predicate | Prohibited | Prohibited |
| Participants, collision, and filter callbacks within physics simulation | Prohibited | Prohibited |
| Object registration callback | Nested addition allowed | Prohibited to avoid freeing an object still registering |
| Object removal callbacks, scene destruction preparation | Prohibited | Reentrant cleanup prohibited |

Physics query `noexcept` callbacks must be read-only. Do not rely on exceptions to exit them or mutate scene membership inside them. `destroy()` only marks, but query/drawing callbacks should still stay read-only. Input snapshots protect current dispatch only; after a callback clears objects, its own code must stop using invalid borrowed pointers.

Update order: `on_before_update` → ordinary objects → UI presentation animation → zero or more fixed steps (scene hook → optional physics simulation) → physics interpolation finalization → optional camera → `on_after_update` → reclaim marked objects. `on_after_update` precedes reclamation; marked objects need not already be freed. The framework attempts final reclamation even if business updates throw.

## Switching, Returning, and Quitting

This fragment belongs in derived scene input/update code. Target 2 must be registered and accept RoomPayload. End the business branch afterward to avoid another request in the same processing cycle.

```cpp
request_scene_switch(2, RoomPayload{42}, elysia::scene::SceneReloadMode::Reset);
return;
```

You may also pass `SceneRoute{.target = ..., .payload = ..., .reload_mode = ...}` to retain return destinations as values. Exit with `request_quit()`. Call these protected interfaces only during normal derived scene input/update, never in `on_enter`, `on_exit`, `on_reset`, drawing, or destructors.

Requests execute after current input/update processing, not immediately at the call. A processing cycle permits at most one request. Duplicate/reentrant requests and invalid/unregistered targets are integration errors and throw; they do not silently select another normal game target. Exceptions may continue into scene failure handling and must not be treated as routing success. See [application failure scene](../builtin-scenes/application-failure.md) for startup and unrecoverable runtime errors.

## Reuse, Reset, and Release

| Route mode | Target behavior |
| --- | --- |
| `Reuse` | Use cached instance, preserve business state, call `on_enter` again; even the current instance exits before entering |
| `Reset` | `Reuse` instance, reset framework runtime, call `on_reset()`, then enter; game chooses how to reset objects/state |
| `Recreate` | Construct a candidate, exit current scene, destroy target's old cache, activate candidate; old object references all invalidate |

Leaving a scene does not destroy it. `on_exit()` detaches external listeners, stops scene sounds, and clears invalid associations; gameplay may retain objects. Do not recreate every object or subscription unconditionally on every entry.

`destroy()` only marks. Scenes reclaim marked objects at update end and after `on_exit()` / `on_reset()`. Objects marked during exit are freed in that exit, without waiting for reentry. UI containers clean children. `on_reset()` does not automatically clear all objects; use `Recreate` when a new object graph is needed.

## Entry, Exit, Failure, and Shutdown Order

Normal entry binds application runtime context/local players, then active effects, object queries, and scene runtime services, then runs `on_enter(payload)`. Thus `on_enter` can create objects, bind controllers, and request current-scene effects. Constructors cannot assume active services point to the candidate.

Normal exit removes route observers, resets input interaction, unbinds runtime services and active queries/effects, calls `on_exit()`, reclaims marked objects, cancels camera paths/blends, then enters Inactive. Screen effects clear on exit. `on_exit` must not rely on active services pointing here; clean associations through retained runtime objects. Undestroyed objects may remain cached.

`Reset` completes before entry: reset input, resume, clear accumulated fixed time/ticks, reset cameras, invoke runtime reset hooks, then `on_reset()` and marked-object cleanup. `GameplayScene` runtime reset releases scene-scoped controllers and updates context generation; other business objects are not automatically rebuilt.

`Recreate` constructs its candidate before current-scene exit. Constructors should establish internal state only; connect external services in `on_enter`. Only successfully entered candidates enter the cache and become current; failed candidates undo bindings and clean up.

Runtime exception recovery unbinds/exits the current scene, then destroys the faulted scene and its objects. Only successful cleanup permits failure-route factory invocation and recovery scene construction/entry. If any cleanup throws, required later cleanup is still attempted, but the final state is Faulted and the factory is not called. Failed recovery also enters Faulted without recursive recovery scenes. Underlying rendering failures go directly to application fault exit.

If `on_enter` throws, `on_exit` is not called as compensation. The engine rolls back its bindings and cleans the candidate. Game-side subscriptions, borrowed callbacks, audio, and other side effects require RAII or local rollback within entry; relying solely on `on_exit` is insufficient.

Application shutdown exits the current scene, then prepares every cached scene for destruction: notify removal while objects live, unbind control/physics, clean registrations, release ownership, then clear scene runtime context. Only after all scenes are destroyed does the application release context, fonts, content resources, Renderer, window, and SDL. Destructor fallback disconnects references/releases resources without calling game virtual lifecycle callbacks; normal logic cleanup belongs to these managed flows.

## References

- [Scene interface](../../../../engine/scene/scene.h), [scene routes](../../../../engine/scene/routing/scene_route.h)
- [GameplayScene](../gameplay/scene.md), [back to user guide](../README.md)
