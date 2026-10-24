# Controllers, Commands, and Actors

This document separates the service, receivers, and scheduling manager. Creation/query/configuration enter through Service; fixed-step scheduling produces intent/delivers commands.

## Game API

[ControllerService](../../../../engine/gameplay/control/controller_service.h) is the game singleton entry. Manager is sole owner; Service stores/schedules no duplicate instances.

Include `engine/gameplay/control/controller_service.h` to call the service through `ELYSIA_CONTROLLERS->method(...)`. The macro expands to `(::elysia::gameplay::ControllerService::instance())`; it does not start a session or change return types or result-handling requirements.

| Interface | Purpose |
| --- | --- |
| begin_session / end_session / session_active | Explicit local game sessions |
| create<T>(create_info, args...) | Create/register, returning expected<`ControllerHandle`, `ControllerError`> |
| get<T>(handle) / describe(handle) | Borrowed queries/binding descriptions |
| remove(handle) | expected<void, `ControllerError`>, immediately stop subsequent delivery |
| bind_target / unbind_target | `ControllerOperation`, explicit binding/unbinding |
| replace_input_map | `ControllerOperation`, controlled local map replacement |

All Service calls/callbacks run on the engine main thread. Future networking workers must queue there, not call directly.

`ControllerCreateInfo` explicitly specifies scene/Session scope; scene scope also needs `control_context().token()`. Failed creation never implicitly starts sessions. Do not retain borrowed pointers across callbacks/removal/session boundaries.

## ControlCommand

Commands contain controller handle, binding generation, sequence, actual tick, continuous state, ordered events, and this step's deltas. Generic commands require no `LocalPlayerId`; identity belongs to `LocalPlayerController`.

Actors implement `ControlCommandReceiver::on_control_command(command, fixed_delta)` / `on_control_cancelled(reason)`. Cancellation clears movement/charge intent, not normal release attacks.

The engine has no fixed Move/Jump/Attack accessors. Demos use game [CommandView](../../../../game/gameplay/control/command_view.h). Built-in `EngineCharacter` explicitly receives its move ID without depending on demo actions.

Physical actors record intent then apply force in participant fixed_update. Nonphysical actors can move with fixed delta in command handlers. Animation `Updatable` remains variable-step without repeated movement.

## Custom Sources

`Controller::produce_intent(tick, fixed_delta)` delivers through protected `submit(ActionInputResult)`. See [custom controllers](../../user-guide/gameplay/control.md#custom-controllers).

## Configuration Operation Results

`ControllerOperation` has no bool conversion. It provides status/error and pending/succeeded/failed queries with Pending/Succeeded/Failed states. Outside callbacks it returns terminal states after execution; inside callbacks Pending means accepted for deferred commit only.

See [controller guide](../../user-guide/gameplay/control.md) for retaining/polling results.

Valid rebind/unbind requests supersede pending old target requests on the same controller, failing them Superseded; rejected requests do not supersede. Map replacements commit FIFO. Target/player exclusivity applies while queued. Removal fails requests InvalidHandle, expired contexts InvalidContext, target removal InvalidTarget, session end NoSession. Every request terminates.

Callback exceptions fail operations CallbackFailed and unbind affected controllers. Their other pending requests also fail; other controllers continue committing. Original/cleanup exceptions then reach scene boundaries preserving render backend priority. end_session propagates notification exceptions only after invalidation/cleanup; begin inside callbacks returns SessionEnding.

Results share small state only, without owning controllers/scenes. Discarding does not cancel, and completed results remain queryable across sessions. No global history/completion callbacks exist. submit rejects during cancellation. Cancellation during map observation/intent production prevents old input being reinserted.
