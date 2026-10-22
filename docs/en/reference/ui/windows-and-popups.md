# Windows, Popups, and Tooltips

This page combines ownership, registration, priority, and detachment for `window` surfaces. Higher drawing `order` alone supplies no `modal`/popup behavior.

See [UI guide](../../user-guide/ui/README.md#create-window) and [index](README.md#browse-by-class).

<a id="surface-contract"></a>
## Shared Surface Registration Rules

| Surface | Ownership/registration | Interaction |
| --- | --- | --- |
| Overlays/dialogs | Window directly owns; registration adds behavior only | Optional modality; close attempts previous scope restoration |
| Transient popups | Implementer owns content; `window` borrows protocol | Active popup input/drawing priority |
| Tooltips | Component owns content/borrows trigger | Passive display, not interactive dialogs |

`UiTransientPopup` supplies `popup_owner`/`is_open`/`contains_popup_point`/`on_popup_input_event`/rendering. `UiDropdown` is the standard implementation. Active popups may block background tooltips.

Overlays/`UiDialog`/`UiConfirmationDialog` must first be direct `UiWindow` children; nested containers reject. Dropdowns may be nested in `window` subtrees. Registration transfers no ownership. Window destruction, unregistration, or subtree removal invalidates borrowed `window` pointers and calls `on_window_detached`.

```cpp
auto dialog = std::make_unique<elysia::ui::UiDialog>(dialog_rect);
elysia::ui::UiDialog* dialog_ptr = dialog.get();
window.add_child(std::move(dialog));
// Open as a popup only after successful overlay registration.
if (dialog_ptr->register_with_window(window))
{
    dialog_ptr->open();
}
else
{
    ELYSIA_LOG_ERROR("ui", "Dialog must be a direct child of its window.");
}
```

Overlays use `open_overlay`/`close_overlay`; dialogs/dropdowns/tooltips use their own open/close/registration. Do not mix them.

The fragment requires dialog/log headers and caller-provided `window`/`dialog_rect`.

<a id="ui-window"></a>
## UiWindow: Window and Surface Coordination

Header: [ui_window.h](../../../../engine/ui/window/ui_window.h). Owns root subtree, coordinates focus/overlays/popups/tooltips.

Main APIs:

- Style: `set_base_style`, `set_style_overrides`, `set_hover_focus_enabled`, `set_on_cancel`.

Cancel first lets the focused control end local adjustment, then invokes the window cancel callback if unhandled. Overlays likewise dispatch to their focused control before applying `close_on_cancel`, without forwarding Cancel to the background window. Transient popups retain their own cancellation priority.
- Focus: `register_focus_scope`, `unregister_focus_scope`, `set_scope_neighbors`, `set_focused_scope`, `focus_first_available_scope`.
- Overlays: `register_overlay`, `unregister_overlay`, `open_overlay`, `close_overlay`, `is_overlay_open`, `overlay_options`.
- Popups/tooltips: `register_transient_popup`, `unregister_transient_popup`, `activate_transient_popup`, `register_tooltip`, `unregister_tooltip`.

See [surface examples](#surface-contract). Dialogs/overlays require direct children; scopes/dropdown popups permit nested subtrees. These registration requirements differ.

<a id="ui-window-调用流程"></a>
### Call Flow

1. Scene creates/owns `UiWindow`.
2. Build ordinary children through `add_child`/`create_child`.
3. Register independent scopes and directional `UiFocusScopeNeighbors`.
4. Transfer dialogs/overlays before registration.
5. Scene sends input/update/drawing each frame.

Focus registrations follow subtree ownership. Temporarily hiding/deactivating windows/scopes/ancestors preserves registration/`order`/adjacency. Unavailable scopes skip selection/navigation; after recovery, existing restoration works without registration and retains valid history.

Cleanup removes only destroyed/out-of-subtree scopes and corresponding current/history caches. Permanently detach through `unregister_focus_scope`.

`register_overlay` returns `false` for destroyed/non-direct children; duplicates replace options. `overlay_options` returns null or borrowed internal options; registry mutations may invalidate, so requery. Placement changes require layout invalidation. Open/close through methods to synchronize focus/visibility, not just the open field.

### UiOverlayOptions: Placement and Closing

These are struct defaults; wrappers differ below. See [ui_overlay.h](../../../../engine/ui/window/ui_overlay.h).

| Field | Default | Meaning |
| --- | --- | --- |
| open | `true` | Initial registration state, separate from lifetime |
| `modal` | `true` | Block ordinary underlying content |
| `close_on_cancel` | `true` | Cancel closes |
| `close_on_outside_click` | `true` | Outside click closes |
| `placement` | `UiOverlayPlacement::Center` | Also `LeftDrawer`/`RightDrawer`/`TopSheet`/`BottomSheet` |
| `transition` | `None` | Optional `Slide` presentation |
| `fallback_size` | `{360, 220}` | Without usable explicit size |
| `order` | `1000` | Written to element on registration |

<a id="dialogs"></a>
## Overlay Dialogs: UiDialog and UiConfirmationDialog

Dialogs temporarily show information/request confirmation. Windows own/schedule them; dialogs maintain content/open state.

Headers: [ui_dialog.h](../../../../engine/ui/composites/ui_dialog.h), [ui_confirmation_dialog.h](../../../../engine/ui/composites/ui_confirmation_dialog.h).

Add directly → check `register_with_window`(`window`,options) bool → open/close. Nested dialogs fail even under the same ultimate `window`. Unregistered/detached/`window`-destroyed open/close safely do nothing.

- `UiDialog`: `set_title_content`, `set_body_content`, `set_action_content`; `set_body_scroll_enabled`/`set_header_visible` configure reading.
- `UiConfirmationDialog`: `UiConfirmationDialogConfig`/`set_config` for title/message/confirm/cancel/close; `set_on_confirm`. Close before callback.

Options exactly equal to default construction cause `UiDialog` to use current style `overlay_defaults`. `UiConfirmationDialog` instead defaults closed/`modal`/cancel-close/no outside-close/center/no `transition`. Explicitly configure other policies. Current dialog size also contributes fallback size.

<a id="ui-dropdown"></a>
## UiDropdown: Dropdown Selection

Header: [ui_dropdown.h](../../../../engine/ui/composites/ui_dropdown.h). Select through transient lists.

<a id="ui-dropdown-配置与打开"></a>
### Configuration and Opening

`set_options`/`add_option` then `set_selected_index`; join `window` subtree before `register_with_window`. open/close/`toggle`/`is_open` control. `set_on_selection_changed` receives the selected index. `unregister_from_window` detaches.

Disabled options remain visible without keyboard/gamepad focus. Without valid options/registration, open does nothing. `set_options` rebuilds rows/preserves old index if valid/enabled, otherwise first available; none clears selection/closes. Retention is by index, not business identity. `set_options` itself does not notify; valid changed `set_selected_index` does, so programmatic setters are not universally silent.

<a id="ui-tooltip"></a>
## UiTooltip: Passive Hints

`UiTooltip` provides trigger descriptions, not interactive dialogs. Header: [ui_tooltip.h](../../../../engine/ui/composites/ui_tooltip.h).

`bind_trigger` borrows; `set_content` acquires; `release_content` returns ownership. `set_show_delay` then `register_with_window` observes; unregister stops.

open/close/`is_open` explicitly control. Borrowing extends no trigger lifetime. Tooltip/trigger normally belong to the registered `window`'s live subtree; maintenance clears departed triggers/registrations. Content remains exclusively tooltip-owned. See [surface rules](#surface-contract).
