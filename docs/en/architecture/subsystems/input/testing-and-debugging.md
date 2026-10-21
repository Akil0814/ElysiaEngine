# Input Testing and Debugging

This document describes automated input-chain coverage and checks with actual devices. The latter half preserves dated acceptance records: they describe what was verified then, not the current working tree or untested hardware.

## Automated Validation

- `input_regression_tests`: small-axis capture, const gating queries, floating-point wheel values, map validation, single cancellation, safe rebinding, supersession, deferred maps, and request completion.
- `input_safety_tests`: UI receiver addition/removal during dispatch, object address reuse, deferred commit callback exceptions, terminal operation states, batch session/pause/context cleanup, multiplayer and UI cancellation failures, pending cancellation after routing failure, destructor resource shutdown, render backend error priority, partition ID allocation after batch configuration, and shortcut frame device scope.
- `controller_runtime_tests`: explicit sessions, scopes, context generations, callback mutations, custom sources, map replacement, and mouse deltas; keyboard partition conflicts, configuration versions, map permissions, and mouse transfer.
- `input_system_controller_lifecycle_tests`: independent buttons, sticks, and triggers for multiple gamepads; concurrent keyboard/mouse; removal and focus-loss notifications.
- `gameplay_scene_input_tests`: player binding, target exclusivity, quick taps, zero/multiple fixed steps, recovery after suppression, rebinding, destruction, queue overflow, and pre-physics-step callbacks.
- `development_input_capture_tests`: development panel capture, text box capture propagation, per-player isolation, UI control transfer, raw-operation consumption, and focus recovery.
- `input_action_mapping_tests` and `engine_gameplay_tests`: action registration, rebinding, axes, and game command views; default gameplay actions are not an engine-wide protocol.
- Existing pointer coordinate, UI, scene, physics, and actor tests continue covering migrated call relationships.

```powershell
ctest --test-dir out/build/sdl3-Debug -L input --output-on-failure
```

Automated tests reproduce multiple devices with controlled SDL events and need no physical gamepad. Actual dual-gamepad acceptance must be confirmed separately on hardware; synthetic input cannot establish hardware conclusions.

## Troubleshooting Order

1. Check source IDs and physical states in snapshots; ensure different gamepads are not mixed together.
2. Check keyboard partitions in `configuration()`, mouse/gamepad ownership through `owner(source)`, and actor bindings. Unbound input does not control actors.
3. Check the UI gamepad, interaction mode, capture categories, and neutral gates; observe whether UI consumes the input.
4. Check `ControllerHandle`, binding generation, local action maps, and command events/deltas; distinguish continuous values from one-shot actions.
5. Check actual fixed steps and cancellation reasons; do not infer execution counts from rendered frames.

Command events are consumed once per fixed step; multiple clicks in one frame retain multiple events. A key still held after a menu closes requires release before recovery; this is deliberate behavior. On queue overflow, first check whether simulation has stopped stepping for a long period or input is being injected repeatedly by mistake.

## Two-Player Example and Hardware Acceptance

Enter **Local multiplayer / input routing** from Demo Gallery. WASD controls P1's blue square by default; arrow keys control P2's red square. Both players work without joining with a gamepad. The mouse belongs to P1 by default. Pressing Start on an unbound gamepad joins P2; arrow keys remain usable, and joining does not open the menu.

Open **Players / menu** to swap keyboard schemes, transfer the mouse, or select **Bind next Start controller to player 1** or **Rebind player 2 controller**, then press Start on an unbound gamepad to assign it. Use **Release controllers for reassignment** before swapping bound gamepads. Disconnecting unbinds only that instance; reconnecting does not restore by name.

UI pad buttons independently select the shared UI gamepad without changing game player bindings. Keyboard and mouse can always operate the menu. Opening the menu enables Navigation and blocks all players; closing restores Pointer. Text boxes capture the whole keyboard, so neither partition can move. Held keys must be released, and sticks/triggers must return to neutral before recovery.

The status bar shows partitions, mouse ownership, gamepad connections, and UI gamepad separately. Hardware acceptance should cover simultaneous keys for two keyboard players (subject to keyboard hardware limits), keyboard/mouse plus gamepad, two gamepads, hot-plugging, text capture, and UI gamepad switching. Current automation uses controlled SDL events; actual dual-gamepad and keyboard multi-key hardware acceptance has not been performed.

## Previous Baseline Record (Not Current Validation)

2026-09-16, Windows / MSVC Debug, build directory `out/build/physics-scenarios-msvc`:

- Full build succeeded.
- Full CTest: 120 / 120 passed, including Input, UI, scenes, Physics, Builtin, examples, and physics stress tests.
- The two-player example was checked with controlled SDL input and software-rendered screenshots: visible actors, joining without opening the menu, independent movement, and modal menu gameplay suppression.
- Relative links in changed documents and `git diff --check` passed.
- Actual dual-gamepad interaction acceptance was not performed; no network session was implemented or verified.

## Controller Subsystem Refactoring Historical Validation

2026-09-17, Windows / MSVC Debug, build directory `out/build/physics-scenarios-msvc`:

- Full build succeeded; `controller_runtime_tests` was added in this round. Final full CTest: 121 / 121 passed in 112.62 seconds; log: `out/controller-full-tests-confirmed.log`.
- In the preceding full run, `scenario_stress_contacts_2` timed out and the other 120 tests passed. It subsequently passed alone (53.66 seconds) and in the final full rerun (46.85 seconds). The timeout cause was undetermined; no timeout threshold was changed and no stress test was skipped.
- Automation covered explicit sessions and handle invalidation, scene/session scopes, callback creation/removal/rebinding, custom non-device controllers, map replacement, fixed-step consumption, and mouse deltas.
- Additional checks confirmed that ordinary HUD consumption of mouse movement clears only the corresponding pending delta, preserving unconsumed wheel and button events. Non-finite command values and previous event values are rejected.
- The query tool responds to keyboard and gamepad through the same active maps; checks covered zero-step waiting, no repeated execution across multiple steps, and binding after cached scene reentry.
- `demo_scene_tests` was rerun with controlled SDL input and software-rendered screenshots exported to `out/controller-routing-qa`. Two-player gameplay, the shared modal menu, and the multi-target camera were inspected; ownership hints, actors, and controls displayed correctly.
- All 28 local links in affected documents were valid. Scans found no old player commands, input broadcasts, scene control target interfaces, or exclusive active-gamepad fields. `git diff --check` passed.
- Actual dual-gamepad interaction was not performed. Synthetic SDL input and software rendering do not constitute hardware acceptance; this round contained no ENet session or network replication implementation.

## Keyboard Partitions and Independent UI Routing Record

2026-09-17, Windows / MSVC Debug:

- Independent keyboard, mouse, and gamepad snapshots; default dual keyboard partitions, conflict rejection, configuration version changes, map permissions, mouse transfer, and text capture were added to automated validation.
- Full build succeeded; final full CTest: 121 / 121 passed in 100.55 seconds; log: `out/partition-full-tests.log`.
- Eleven targeted tests passed. The two-player example additionally checked swapping keyboard schemes through the menu, independent mouse transfer, and restoration of external keyboard/mouse configuration on exit.
- Software-rendered screenshots are in `out/keyboard-partition-qa`; two-player gameplay and the complete shared menu were inspected. Menu layout avoids the ownership hints at the top.
- All 17 local links in input, UI, and online design documents were valid. Scans found no old combined keyboard/mouse source, UI player owner interface, or old default map entry points. `git diff --check` passed.
- Actual dual-gamepad and keyboard multi-key hardware acceptance was not performed. This round used controlled SDL input and contained no network implementation.

## Input and Controller Interface Consolidation Record

2026-09-18, Windows / MSVC Debug:

- Added `input_regression_tests` for small-axis capture, query-only gating, invalid map enums/components, floating-point wheel values, cancellation deduplication, targets invalidated by rebinding callbacks, supersession, reservations, session termination, and discarding old input from callbacks.
- UI scroll containers checked fractional and reverse scrolling. The two-player example checked device restoration when the session ends before scene closure; shutdown errors fail the test.
- ImGui enabled: final full engine and example build succeeded; full CTest 122 / 122 passed. Log: `out/input-audit/full-tests.log`. The final workspace configuration was restored to enabled.
- ImGui disabled: full engine and example build succeeded; 20 / 20 targeted input, UI, controller, example, and application lifecycle tests passed. Log: `out/input-audit/no-imgui-tests.log`.
- Two-player gameplay, menu capture, device transfer, rebinding, and scene reentry were checked with synthetic SDL events. Software-rendered screenshots are in `out/input-audit/screens`; the two-player menu and multi-target camera were inspected.
- Physical gamepad hot-plugging, actual dual gamepads, and high-precision touchpad scrolling were not accepted on hardware; synthetic input and screenshots cannot replace that acceptance.
