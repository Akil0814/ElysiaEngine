# Scene Lifecycle and Safety Boundaries

This document explains state transitions, ownership, and exception cleanup for developers modifying the scene framework. Focus on who is responsible at each stage, when collections may change, and which steps must still complete after failure.

`SceneManager` coordinates transitions, caching, and exception recovery. `Scene` owns objects and implements non-overridable input, update, rendering, and cleanup flows. Games implement `on_enter`, `on_exit`, `on_reset`, and dedicated extension hooks. Examples and the complete allowed-mutation phase table are in the [scene guide](../../user-guide/scene/scene.md).

## States and Ownership

`Scene` states follow Inactive → Entering → Active → Exiting → Inactive. Reset only moves from Inactive through Resetting back to Inactive. Destruction preparation moves from Inactive through PreparingDestruction to PreparedForDestruction; re-entry is then prohibited. A Running `SceneManager` dispatches input, update, and rendering only to the current scene. Cached Inactive scenes do not update.

`SceneFactory` owns a `unique_ptr<Scene>` per SceneKey; different keys may use the same concrete type. Scenes exclusively own world objects and UI roots; UI containers exclusively own their children. Borrowed references, control targets, physics registration, listeners, and service bindings do not extend owner lifetimes.

Default scenes create no fixed-step runtime, physics, or camera. Configuring `PhysicsWorld` also requires `FixedStepRuntime`. `GameplayScene` enables fixed steps and a control context by default; gameplay collision requires explicit enabling and physics. Runtime context borrows the renderer, content registry, fonts, and development panels from `Application`; these must outlive all cached scenes.

## Routing and Callback Order

| Operation | Order |
| --- | --- |
| Create / enter | Construct candidate → bind context, players, and UI device access → bind active effects, queries, and scene runtime → bind request observer → `on_enter` → cache candidate and commit current scene |
| Leave | Remove request observer → reset input interaction → detach scene runtime → detach queries and effects → `on_exit` → reclaim marked objects → cancel camera activity → Inactive |
| Reuse | Leave current scene → enter existing target; preserve undestroyed game objects, rebind control targets |
| Reset | Leave current scene → reset input / pause / fixed steps / camera → `on_runtime_reset` → `on_reset` → reclaim marked objects → enter target |
| Recreate | Construct candidate first → leave current scene → destroy old target cache → bind and enter candidate → cache and commit |
| Destroy cached scene | Ensure Inactive and detached services → cancel camera activity → logically retire all objects → reset input → clear runtime context → destruct |

Target camera slots reset on entry across instances and on Reset/Recreate. Reuse of the current instance cancels active paths and blends but preserves static poses. Slots not declared as owned do not reset. Configure cameras and external services in `on_enter`, never by modifying services still used by the current scene during candidate construction.

Failed `on_enter` does not receive a compensating `on_exit`: the candidate only reached Entering, so the manager undoes engine bindings and destroys it. Undo game-side subscriptions and callbacks with RAII or local rollback. During normal exit, runtime services are already detached; use interfaces owned by this scene for cleanup, rather than assuming global active services still point here.

## Update and Object Mutation

An update runs `on_before_update` → ordinary object updates → UI presentation animation → fixed steps → physics `finalize_frame` → camera → `on_after_update` → reclamation. Each actual `GameplayScene` fixed step delivers control commands, runs `on_game_fixed_update`, then enters optional physics simulation.

`add_object` synchronously accepts ownership, registers interfaces and physics, then calls `on_scene_object_registered`. Failed registration rolls back that object. Registration hooks may add independent objects recursively, but the entire registration process prohibits clearing the scene, which could release an object currently being registered. Synchronous addition/removal is prohibited during object update, UI presentation, rendering, object queries, and physics simulation. Before/after-update hooks, scene fixed-step hooks before simulation, and input snapshot callbacks may mutate collections. Physics query `noexcept` callbacks must be read-only.

`destroy()` marks an object. Reclamation runs after update, exit, and reset; leaving does not require waiting for another frame. Retirement order is: notify removal while alive → unregister physics and detach participant → clear input/update/physics registrations → release the `GameObject` or UI-root owner. `GameplayScene` unbinds control targets and gameplay collision before the game removal extension. One object's cleanup exception does not stop retirement of other objects in the batch.

## Exception Recovery and Shutdown

`Scene` logic failures record the original boundary, detach and exit the scene, then destroy the faulted scene and its objects. Only after all necessary cleanup succeeds does the manager invoke the failure-route factory and construct and activate recovery, preventing concurrent ownership of external resources by recovery and faulted objects. Cleanup exceptions do not stop executable cleanup; diagnostics are collected, state becomes Faulted, and the route factory is not called. Failed recovery enters Faulted without recursion.

Preserve ordinary business exceptions and later cleanup diagnostics. A `RenderBackendError` takes precedence and propagates to application fault exit; the same renderer is not used to display recovery UI. Shutdown stages are isolated, failure results are sticky, and repeated shutdown cannot replace failure with success.

`Application` first stops input and shuts down `SceneManager`. The manager exits the current scene, clears debug and screen effects, resets cameras, destroys cached scenes, then closes controllers and clears application-level players. `Application` then closes development panels and releases runtime context, saves, fonts, project resources, and built-in resources; renderer, window, and SDL come last. Destructor fallback only releases resources and detaches borrows; it never calls callbacks of already destructed game-derived classes.

## Regression and Example Validation

Core regressions are in `tests/scene/scene_core_tests.cpp`, `scene_lifecycle_regression_tests.cpp`, and `scene_factory_failure_tests.cpp`; integration regressions cover controllers, cameras, and `Application` shutdown.

`tests/scene/compile_scene_documentation.cmake` extracts complete scene, object, and service examples directly from Markdown for `scene_documentation_examples` (Chinese) and `scene_documentation_examples_en` (English). Both use the same extraction list. `#line` maps diagnostics to original locations. Markdown changes trigger extraction; no separately copied examples are maintained.
