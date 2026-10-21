# Input Architecture and Lifecycle

This document describes the entire chain from device snapshots to actor commands, including ownership and lifetime at each layer. When maintaining input code, first identify the responsibility, then check how input consumption, controller binding, and fixed-step delivery relate.

## Responsibilities

| Module | Responsibility |
| --- | --- |
| `InputSystem` | Stores physical state, start/end-of-frame state, ordered events, deltas, and device changes per device; outputs snapshots only |
| `GamepadDeviceManager` | Opens and closes SDL gamepads and manages device instances |
| `SceneInputRouter` | Independent UI device access, raw operation consumption, capture, shortcuts, player routing, and release/neutral gating |
| `ControllerService` | Game-facing session, creation, query, `configuration`, binding, and removal entry points |
| `ControllerManager` | Sole owner of controllers, command caches, handles, and fixed-step scheduling; Service keeps no copies |
| `SceneControlContext` | A specific scene instance and generation, target membership, activity, and simulation boundaries; owns no controllers |
| `Controller` | Source-independent contract for producing intent |
| `LocalPlayerController` | Converts local player snapshots into game actions |
| `ControlCommandReceiver` | Lets an actor interpret actions and execute gameplay |

## Directories and Dependencies

| Location | Contents and boundary |
| --- | --- |
| `engine/input/` | Device collection, raw snapshots, capture/gating, and player device ownership; no scene or UI coordination implementation |
| `engine/input/translator/` | Converts SDL events into engine raw input |
| `engine/input/action/` | Generic actions, maps, and action frames; usable by ordinary scenes and controllers |
| `engine/ui/input/` | Converts raw input to UI actions and events, synthesizes scrolling, and defines receiver contracts |
| `engine/scene/input/` | `elysia::scene::SceneInputRouter` coordinates scenes, UI, player cancellation, and consumption; `UiInteractionMode` and `UiDeviceAccess` also belong to `elysia::scene` |
| `engine/gameplay/control/` | Source-independent controllers, commands, ownership, and binding; `SceneControlContext` lifetime implementation is separate from the Manager source file |
| `engine/gameplay/scene/` | Integrates controllers and optional collision runtime into scene lifetime |
| `game/input/` | Game action IDs, keyboard schemes, and default action maps in namespace `example::input` |
| `game/gameplay/control/` | Game controller integration and semantic command views in namespace `example::gameplay` |

Base input does not depend on `Scene`/UI implementations. UI input and scene input coordination read base input. Controllers depend on generic actions; `GameplayScene` connects scenes and controllers. Game action definitions stay outside the engine layer.

The game-side [local_controls.h](../../../../../game/gameplay/control/local_controls.h) declares only `configuration`, creation, and query entry points. Instance bindings and the session handle table live in the corresponding `.cpp`; external code cannot modify the table directly. [CommandView](../../../../../game/gameplay/control/command_view.h) interprets game actions and remains a lightweight read-only view. `tests/input/` organizes tests around the whole input chain, including integration regressions across UI, scenes, and controllers.

## Module Integration

`ControllerService` and `ControllerManager` reuse the engine's `elysia::tools::Singleton<T>` and its inherited `instance()`. Copying and moving are prohibited; constructors remain private, and only the `Singleton` template can create instances.

Ordinary scenes have input routing and UI, but create no controller context. `GameplayScene` supplies context and scheduling integration without automatically creating any controllers. The game chooses their number, type, maps, and targets.

## Devices and Players

Physical sources are `InputSourceId::keyboard()`, `mouse()`, and `gamepad(instance)`. `InputSourceKind` expresses the category; numeric ranges do not. There is one logical keyboard and one logical mouse, without distinguishing multiple physical keyboards or mice. Each gamepad has independent translation and trigger thresholds. UI tracks its own active device for prompts; `InputSystem` offers no global current_device query.

`SceneManager` owns an application-level `LocalPlayerRegistry`. P1 defaults to a full keyboard partition and the mouse. A keyboard partition is game `configuration`, not a physical device. Each player can have at most one partition, one mouse source, and one gamepad. Partitions cannot share keys; the entire mouse is exclusive. Player identity is independent of devices and actors.

`create_partition` / `update_partition` / `remove_partition` manage partitions; `bind_keyboard` binds a partition. `bind_source` / `unbind_source` / `transfer_source` manage the mouse or a gamepad; `replace_gamepad` replaces a player's gamepad.

`create_partition` returns `std::expected<KeyboardPartitionId, InputBindingError>`. Save the successful new partition ID for later binding, updates, or removal. Mutations such as partition updates/removal and device binding return `std::expected<void, InputBindingError>`. Repeatedly unbinding a valid unassigned device succeeds; `.error()` identifies invalid players or devices.

`configuration` provides read-only `configuration`. `apply_configuration` returns `std::expected<void, InputBindingError>`: success means the batch was committed atomically; failure leaves bindings and versions unchanged. Bound partitions cannot be removed, and active controllers' keyboard maps cannot reference keys outside their partition.

Each player's binding_version changes when ownership or partition contents change. Checks occur during input and before actual fixed steps. Affected players cancel old commands and must release held controls or return them to neutral; other players continue. Mouse transfer discards pending deltas and does not synthesize movement from its current position. Removing a gamepad unbinds only that instance; reconnecting does not restore it automatically.

Buttons from a player's sources are unioned; continuous axes are summed and clamped by the map. Partitions `filter` both start/end-of-frame state and ordered events. UI consumption always uses raw physical sources and event identifiers before routing to players.

## Lifecycle

`SceneManager` initializes Manager on the engine side, ends the session on shutdown, and deactivates the runtime. Before initialization or after shutdown, begin_session returns NotInitialized.

All controllers belong to an explicit game session. `begin_session()` never silently replaces an existing session; creation fails without one. `end_session()` first stops delivery and invalidates all handles, then cancels and releases every controller. If a cancellation callback throws, its target is still notified and the other controllers are cleaned up before the exception propagates. Starting a session inside cleanup callbacks returns `SessionEnding`. Application shutdown also cleans up. A game session bounds local object lifetimes and has no connection to ENet or connections.

| Scope | Creation requirements | Leaving a scene / Reuse | Reset / Recreate / destruction |
| --- | --- | --- | --- |
| `Scene` | Active session and valid owning context generation | Cancel and unbind; retain instance and `configuration` | Release owned controllers |
| `Session` | Active session | Cancel and unbind; retain instance and `configuration` | Retain instance; clear target and old context references |

A scene token contains instance identity and generation; SceneKey cannot replace it. Reset increments the generation; Recreate creates a new instance. The game must explicitly bind the new scene; no same-named actor is found automatically.

`ControllerHandle` contains a runtime generation and instance number. Removal and session end invalidate old handles; new sessions never reuse a valid old handle. Keep handles for long-term references. `get<T>()` returns a borrowed pointer that must not be retained across scheduling mutation boundaries.

A controller has at most one target, and a target at most one controller. In the same active scene, a player can have at most one bound local controller; unbound instances and instances in cached scenes may coexist. Targets must be live objects registered in the currently active context. Validation failure before commit preserves the original binding. Once the old target has received cancellation, a callback that invalidates the new target leaves the controller unbound and reports failure; old commands are not restored.

Removal inside a callback immediately stops subsequent delivery; destruction waits for a safe boundary. New instances created in callbacks participate no earlier than the next actual fixed step. Rebinding, unbinding, and map replacement inside callbacks return Pending operations and commit at safe boundaries. Validation occurs at enqueue and commit, and again after cancellation callbacks. Object removal notifications immediately fail pending binding requests. Leaving, rebinding, and reset clear states, events, and deltas and advance the binding generation.

Commit is an explicit scheduling phase; destructors do not execute user callbacks. Callback exceptions during commit terminate the operation as `Failed / CallbackFailed`, unbind the affected controller, cancel its other pending operations, and continue processing other controllers' requests before propagating to the scene exception boundary. Context reset advances the generation and clears old references even when notifications fail. Destructors provide only resource shutdown fallback without user callbacks. The engine internally maintains cancellation handlers and keyboard map permission constraints; the game cannot replace or clear them.

## A Frame and a Fixed Step

1. `SceneManager` invokes the scene's internal lifecycle input entry point, delegates to `SceneInputRouter`, and maintains physical sources and device changes.
2. UI accepts keyboard, mouse, and the designated UI gamepad independently of game bindings; capture is rechecked before and after each operation.
3. Unconsumed operations reach device joining and scene shortcuts. Shortcuts declare device categories through set_shortcut_devices; both frame state and events are filtered to that scope, then consumed with consume_input(event).
4. Remaining snapshots reach the corresponding `LocalPlayerController`; mapped actions enter the Manager cache.
5. For every actual step of the scene's `FixedStepRuntime`: custom controllers produce intent, Manager delivers commands, and the game fixed-update extension runs. Scenes with a `PhysicsWorld` then advance physics participants and simulation.

The scene's `FixedStepRuntime` is the sole owner of the fixed-step accumulator. `GameplayScene` runs controller fixed steps even without a `PhysicsWorld`. Frames with zero fixed steps accumulate events and deltas for a single consumption on the next step; catch-up steps retain only continuous values. Dropped catch-up steps do not consume input. Invalid or inactive targets and inactive contexts accumulate no commands for later recovery.

## GameplayScene Scheduling Seam

`GameplayScene` input, fixed-step, and object-removal integration points are final. Each actual fixed step delivers controller commands, runs the game's `on_game_fixed_update`, then advances physics participants and simulation. The game must not manually drive this chain again.

The scene manager coordinates context activation, exit, and reset. Exit unbinds and cancels input. Reset updates the context generation, releases scene-scoped controllers, then calls the game's `on_reset`; `Session` controllers remain until session end. Resetting UI input routing alone cannot replace world lifecycle operations. See the [GameplayScene guide](../../../user-guide/gameplay/scene.md) for integration and a two-player example.

## UI and Cancellation

Shared UI accepts keyboard, mouse, and at most one independently designated gamepad. set_ui_gamepad does not change game device ownership. A pure menu may claim the first valid gamepad press as its UI gamepad and consume that operation. Gameplay scenes do not claim one automatically.

`Scene` UI frame dispatch and each batch of event dispatch use receiver registration snapshots. Callbacks may add objects or mark them for destruction. New receivers join on the next dispatch; invalid registrations are skipped. Registration numbers distinguish reused addresses, preventing an old snapshot from calling a new object. Consumption priority stays unchanged within an event batch.

Ordinary scenes default to Navigation; `GameplayScene` defaults to Pointer, where ordinary HUD focus does not consume keyboard/gamepad navigation. Use set_ui_interaction_mode(Navigation) when opening an interactive menu and restore Pointer on closing it. A focused text box captures the entire physical keyboard even in Pointer mode, affecting every keyboard partition. Mouse capture affects the mouse's owner; gamepad capture affects only the designated UI gamepad. If any UI event produced by a raw operation is consumed, that operation and its corresponding continuous control cannot pass through to gameplay.

By default capture cancels only affected local controllers. `set_all_gameplay_input_blocked` blocks all local gameplay input without automatically intervening in custom sources such as AI or remote controllers. World `pause()` cancels and stops every controller; UI continues processing.

Batch cancellation for players and UI roots collects callback exceptions, continues notifying other receivers, then propagates to the scene boundary. Resetting UI interaction still delivers an empty frame; routing failure midway still completes queued player cancellation. Internal state cleanup and device unbinding are not skipped when UI cancellation callbacks fail. Multiple failures follow scene diagnostic collection and render backend error priority.

Focus loss, suppression, device rebinding, and target/map changes use cancellation notifications, never fabricated normal release events. Each suppressed key must be released; sticks and triggers must return to neutral (absolute value at most 0.2) before taking effect again. Transferring UI control preserves focus and clears held, drag, repeat, and synthesized scroll state.

## Capture and Gating

Generic `InputCapture` is defined in `engine/input/input_capture.h` and shared by development overlays and UI. During capture, continuous values for matching devices are unconditionally zeroed; axis dead zones cannot bypass capture. After capture ends, existing gates still require key release or an absolute value at most 0.2 on each axis.

`InputSuppression::observe` updates gates only in physical-state order; `filter` is a const query. UI frame precomputation uses a copy of the gates and does not advance the real event-routing gates early. Suppression itself sends no cancellation. Routing combines cancellation per player and notifies before mapping, with reason priority FocusLost, SourceChanged, Suppressed. `Controller` lifetime handles pause, unbinding, and unavailability; continuing unavailability does not notify repeatedly.
