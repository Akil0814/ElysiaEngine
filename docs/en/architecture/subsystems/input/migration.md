# Input and Controller Interface Migration

This document lists implemented interface changes and replacement entry points for maintainers migrating old input code. Check lifetime and return results as well as type names.

The current interfaces retain no compatibility aliases or forwarding from old entry points. Repository examples and tests have migrated.

| Old interface / behavior | New interface / behavior |
| --- | --- |
| DevelopmentInputCapture and old header | `InputCapture`, include engine/input/input_capture.h; use captures_input for bit checks |
| `InputSystem`::current_device, InputDeviceTracker | Removed; collection classifies events, UI maintains its own active device |
| `InputSystem`::end_frame, `GamepadInputTranslator`::reset | Empty lifecycle or unused entry points removed |
| Integer raw/UI wheel values | float; consumers preserve fractions, integer sources convert explicitly or use floating-point fields |
| Boolean success checks for bind_target, unbind_target, replace_input_map | `ControllerOperation`; explicitly check Pending, Succeeded, Failed |
| bool for device binding/unbinding/transfer/replacement | expected<void, `InputBindingError>`; check or log error |
| bool from `ControllerService`::remove | expected<void, `ControllerError`> |
| Capture filtering implicitly advances state | observe updates gates; const filter only reads |
| `LocalPlayerRegistry`::reset | Duplicate entry point removed; use reset_defaults |
| `RawInputState`::clear_keyboard / clear_pointer / clear_gamepad | Unused category clearing removed; physical sources are independent, use clear for a source and clear_control for one control |
| Game modifies cancellation handlers or keyboard permission state | Internal interfaces are no longer game-facing; use scene routing configuration and `ControllerService` map replacement |
| `engine/input/scene_input_router.h` and `elysia::input::SceneInputRouter` | `engine/scene/input/scene_input_router.h` and `elysia::scene::SceneInputRouter` |
| `elysia::input::UiInteractionMode` / `UiDeviceAccess` | `elysia::scene::UiInteractionMode` / `UiDeviceAccess`, following scene routing ownership |
| `game/input/local_controls.h` / `command_view.h` and corresponding `example::input` control entry points | Same-named headers under `game/gameplay/control/`; use `example::gameplay::configure_scene_player`, `session_player`, `existing_session_player`, and `CommandView` |
| Directly modify the session handle table via `session_player_handles()` | Public storage access removed; create/query with `session_player`, or query only with `existing_session_player` |

Configuration calls may queue inside callbacks. Do not treat `!succeeded()` as immediate failure. Callers that must wait retain the result and handle its terminal state during updates. Initialization that only permits synchronous execution should require Succeeded and treat Pending as an invalid call time. The multi-target camera example retains rebinding requests and changes the primary target only after success.

Binding validation failure before cancelling the old target preserves the old binding; discovering an invalid target after cancellation callbacks leaves it unbound. Cancellation is not key release and must not trigger release attacks. Each focus loss notifies FocusLost once, pause notifies Paused only, and continuing unavailability does not notify repeatedly.

Callback exceptions during configuration commit set the operation to `Failed / CallbackFailed`, clean up the affected binding and that controller's pending requests, then propagate to the scene exception boundary. Do not treat `CallbackFailed` as an ignorable ordinary map validation failure. Ending a session invalidates every handle first, then completes cancellation notifications for each controller and target. Notification failure does not prevent other cleanup. `begin_session()` inside cancellation callbacks returns `SessionEnding`; a new session can begin after exception handling for the end call completes.

`set_shortcut_devices` limits both frame state and events passed to on_shortcuts. Keyboard partition IDs introduced by batch configuration are not overwritten by subsequent create_partition calls; exhausted allocation returns `PartitionIdsExhausted`.

Related automated tests use synthetic input. Physical gamepad hot-plugging, high-precision touchpad scrolling, and development overlay use still require hardware acceptance; synthetic tests cannot replace it.
