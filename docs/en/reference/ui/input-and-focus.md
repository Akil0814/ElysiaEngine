# Input, Focus, and Interaction Cancellation

This page covers interaction, focus registration, capture, and cancellation. Standard scenes drive UI input without repeated game dispatch.

See [UI guide](../../user-guide/ui/README.md#create-window) for integration and [reference index](README.md#browse-by-class) for classes.

<a id="navigation"></a>
## Focus Relationships and Coordinates

Windows use `register_focus_scope`/`set_scope_neighbors` across scopes; scopes maintain available targets. Directional actions search neighbors only if the control cannot handle them. Nested scopes retain navigation duties.

See [geometry](elements-and-layout.md#ui-element) for presentation/layout conversion and [UiScrollContainer](elements-and-layout.md#ui-scroll-container) for focus/scroll coordination.

<a id="ui-control"></a>
## UiControl: Interactive Base

`UiControl` adds enabled/focused to `UiElement` and implements `UiFocusable`. Header: [ui_control.h](../../../../engine/ui/core/ui_control.h).

`set_enabled`/`is_enabled` control/query interaction. Focus systems/containers drive `set_focused`/`is_focused`. Derived controls handle confirm/pointer in `on_ui_input_event`.

Scopes/lists/groups/windows maintain focus exclusivity. Setting every control's focus separately does not replace [navigation](#navigation).

<a id="ui-focus-scope"></a>
## UiFocusScope: Focus Scope Contract

`UiFocusScope` describes window navigation regions without owning controls. Ordinary interfaces use concrete containers. Header: [ui_focus_scope.h](../../../../engine/ui/focus/ui_focus_scope.h).

`focus_scope_element` provides the element; `set_scope_focused`/`is_scope_focused` manage scope focus. `focused_target` queries, `focus_first_available` selects, `has_focusable_target`/`can_navigate`/`contains_focus_point` test operations.

Implement directly only for new focus containers. See [windows](windows-and-popups.md#ui-window) for registration.

<a id="ui-control-focus-scope-host"></a>
## UiControlFocusScopeHost: Owning Focus Scope

This base owns children/navigates internally for lists/grids. Header: [ui_control_focus_scope_host.h](../../../../engine/ui/focus/ui_control_focus_scope_host.h).

Use `focus_first_available`, query `focused_target`/`has_focusable_target`/`can_navigate`, or explicitly `set_focused_target`.

Derived containers establish adjacency with `set_focus_entries`; `clear_focus_for_gamepad_scroll`/`restore_focus_after_gamepad_scroll` coordinate scrolling. [Ownership rules](elements-and-layout.md#ownership) still apply.

<a id="capture-and-cancel"></a>
## Input Capture and Interaction Cancellation

`UiElement`::input_capture defaults none; `UiChildHost` combines valid children; focused `UiTextInput` captures keyboards; `UiWindow` modal/active popup layers capture. Keyboard capture spans all partitions, mouse affects its owner, gamepad only the UI gamepad. UI selection is independent of gameplay binding.

cancel_input_interaction clears transient holds/dragging but preserves content/focus for focus loss, disconnect, and UI gamepad changes. It differs from full `reset()`.

This virtual interface permits throwing notifications; state-only controls may still override `noexcept`. Batch cancellation continues for live controls then propagates first exception. HUD [UiActionButton](controls-and-selection.md#ui-action-button) reports active mouse interactions as `Canceled`; external state changes emit no interaction callbacks.

Scene `set_ui_interaction_mode` configures `UiInteractionMode`. Pure menus default `Navigation` and can claim first gamepad press without binding a player. Gameplay defaults `Pointer` for pointer-only HUD; interactive menus explicitly enable `Navigation`. Active text boxes always capture keyboard; suppressed keys require release after editing before gameplay resumes.

<a id="input-runtime"></a>
## Input and Scroll Extension Interfaces

Ordinary UI uses scenes/containers; bridges/new containers directly use below. See [frame architecture](../../architecture/subsystems/ui/README.md#frame-flow).

`UiAction` expresses navigation, `Confirm`, `Cancel`, `Tab`, editing/page actions. `UiInputEvent` type determines valid payload fields. See [ui_input_types.h](../../../../engine/ui/input/ui_input_types.h); irrelevant default payloads are not input.

| Type | Methods | Purpose |
| --- | --- | --- |
| `UiInputRouter` | `route_frame`, `route_event`, `synthesize_events`, `reset_transient_state` | Convert raw frames/events and synthesize gamepad scrolling. |
| `UiInputState` | `set`, `is_pressed`, `is_just_pressed`, `is_just_released` | Held state and current-frame press/release edges. |
| `UiGamepadScrollSynthesizer` | `synthesize(raw_frame)`, reset | Continuous gamepad input to optional scroll events. |
| `UiScrollState` | `set_axis`, viewport/content size, offset, simulation step, ratio, `scroll_by` | Independent scrolling and offset limits. |
