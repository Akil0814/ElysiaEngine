# Buttons, Selection, and Value Adjustment

This page organizes controls by triggering operations, retaining selections, adjusting values, and switching pages. Widgets maintain UI state; hosts implement business effects through callbacks.

See [UI guide](../../user-guide/ui/README.md#create-window) for integration and [index](README.md#browse-by-class) for classes.

<a id="callbacks"></a>
## Common Callback Contracts

set_on_<event> replaces callbacks rather than accumulating them. Callbacks may modify tree state; destruction of current objects, scene switching, or parent replacement should defer until event completion or use higher-level scheduling.

- `set_on_click`: no arguments.
- `set_on_selection_changed`: `UiRadioGroup`/tabs use std::optional<std::size_t>; dropdown uses std::size_t; individual radio buttons no arguments. `UiButtonGroup` has no such callback.
- `set_on_toggled`: `UiCheckboxState`.
- `set_on_value_changed`: `float`.
- Text input std::string_view is valid only during callbacks; copy to `std::string` if retained.

Programmatic notification depends on APIs: Checkbox `set_state` is silent; valid Dropdown index changes notify. Follow specific contracts below, not one universal rule.

<a id="ui-button"></a>
## UiButton: Button

Header: [ui_button.h](../../../../engine/ui/widgets/ui_button.h). Ordinary confirmation/selection with text/icons/state textures.

<a id="ui-button-内容与回调"></a>
### Content and Callbacks

Construct with `UiButtonConfig` or atomically `set_button_config`. `set_text_content` changes text directly. `UiButtonConfig::content` holds `UiButtonIconContent`/`UiButtonTextureSetContent`. `set_on_click` replaces; `prepend_on_click` inserts before existing callbacks.

`set_state_textures`/`set_sounds`/`set_padding` configure presentation. See [styles](style-and-theme.md#style-contract).

`UiButtonContent`/texture pointers transfer no external texture ownership.

<a id="ui-button-交互与状态"></a>
### Interaction and State

Only visible/active/enabled buttons handle confirm/primary mouse. Disabling changes style/prevents clicks. `set_focused` normally comes from scopes; manual focus suits composite visual synchronization, not navigation replacement.

This belongs in the `button` owner's member function; games implement `save_settings()`:

```cpp
button.set_text_content(elysia::ui::ui_text_key("common.confirm"));
// The host validates/applies/saves settings; callbacks must not outlive it.
button.set_on_click([this]
{
    save_settings();
});
button.set_visual_role(elysia::ui::UiButtonVisualRole::Primary);
```

<a id="ui-action-button"></a>
## UiActionButton: HUD Action Slot

Header: [ui_action_button.h](../../../../engine/ui/widgets/ui_action_button.h). Generic HUD skill/item/pause slots. It inherits `UiElement`, accepts mouse only, never focus navigation/keyboard/gamepad `Confirm`, and binds no actions/players/controllers.

<a id="ui-action-button-内容与状态"></a>
### Content and State

Construct with rect, position/size, or `from_center`. `set_content` accepts empty, `UiTextContent`, `UiActionButtonIconContent`. Callers own icons; `source_rect` selects atlas regions. Icons preserve source aspect inside available space; text uses existing localization/fonts.

- `set_key_hint` sets bottom-left, `set_badge_text` top-right, raw/key text. Games format counts/resolve key names.
- `set_selected` controls independent selection border.
- `set_overlay_ratio` masks bottom-up: 0 none, 1 full, finite clamped `[0,1]`, nonfinite 0. No timing or interaction effect.
- `set_external_pressed` is visual only. `is_pointer_pressed` and `is_external_pressed` query sources; `is_pressed` unions them. Mouse release leaves external state unchanged.
- Disabling blocks new mouse input/cancels current. Explicit/system cancellation clears pointer hold, hover, external visuals; games may resupply external state next sync.

Drawing order: background, content, mask, border, selection border, hint, badge. Mask/selection independent of enabled. `set_base_style`/`set_style_overrides`/`clear_style_overrides` configure; theme derives `button` colors preserving overrides. `set_font_source_override` affects all text: Button role for main, Caption for hints/badges.

Commands use layout coordinates; scene/parent applies presentation translation/opacity/clipping. Hits include ancestor translation.

<a id="ui-action-button-交互与生命周期"></a>
### Interaction and Lifecycle

`set_on_interaction` receives `UiActionButtonInteraction`:

| Phase | Meaning |
| --- | --- |
| `Pressed` | Primary press inside interactive control; repeats do not renotify. |
| `Released` | Interaction begun here ends; `clicked` only if still interactive/inside on release. |
| `Canceled` | Disable/explicit/system cancellation ends interaction; not successful click. |

Leaving does not end hold; returning can click. Outside release still notifies `Released` with `clicked=false`. Motion/other buttons are unconsumed. Only owned primary presses/releases are consumed, without capturing entire device categories.

State updates before callbacks. Callbacks may remove current controls, mutate trees, or throw along existing input boundaries. System batch cancellation uses stable snapshots, cleans remaining live controls, then propagates first failure. Newly `added` controls are outside that batch.

reset/destruction invokes no callbacks. Before hiding/deactivating/removing HUD, explicitly cancel_input_interaction and end game actions. Input cancellation/scene lifetime must also clean actions without destructor notifications. Controls may receive paused UI input; games govern availability through enabled/callback logic.

<a id="ui-action-button-游戏层连接示例"></a>
### Game Integration Example

Runnable UI gallery Controls page: [ui_component_gallery_hud.cpp](../../../../game/showcase/ui/ui_component_gallery_hud.cpp). E/mouse starts skills with 3-second cooldown; 1/2/mouse uses items; P/pause only pauses this example's cooldown/items. Switching tabs clears interaction; hidden pages ignore shortcuts.

Games implement these methods, which can submit requests or pause scenes. The control chooses no route itself.

```cpp
using namespace elysia::ui;

// Held skills: HUD input produces requests; external gameplay state updates visuals only.
skill_button.set_key_hint(ui_raw_text("E"));
skill_button.set_on_interaction([this](const UiActionButtonInteraction& event)
{
    if (event.phase == UiActionButtonInteractionPhase::Pressed)
    {
        begin_skill_from_hud();
    }
    else // Released or Canceled: end the HUD hold request in either case.
    {
        end_skill_from_hud();
    }
});
// Game HUD update: do not execute the skill again here.
skill_button.set_external_pressed(skill_action_is_held());
skill_button.set_overlay_ratio(skill_cooldown_remaining_fraction());
skill_button.set_enabled(can_start_skill());

// Item slots: successful clicks change selection; game state supplies count and selection.
item_slot.set_on_interaction([this](const UiActionButtonInteraction& event)
{
    if (event.phase == UiActionButtonInteractionPhase::Released && event.clicked)
    {
        select_item_slot();
    }
});
item_slot.set_badge_text(ui_raw_text(std::to_string(item_count())));
item_slot.set_selected(item_slot_is_selected());

// Pause: keyboard shortcuts and HUD clicks call the same game method.
pause_button.set_on_interaction([this](const UiActionButtonInteraction& event)
{
    if (event.phase == UiActionButtonInteractionPhase::Released && event.clicked)
    {
        toggle_pause();
    }
});
```

For skills cooling on press, separate ability to start from continued hold. Disabling cancels current pointer interaction. Merge multiple source holds in game logic so one release does not terminate another held source.

<a id="selection-controls"></a>
## Basic Selection: UiCheckbox and UiRadioButton

Checkboxes hold independent tri-state values; radio buttons one selected flag. Groups, not individual buttons, enforce exclusivity.

Headers: [ui_checkbox.h](../../../../engine/ui/widgets/ui_checkbox.h), [ui_radio_button.h](../../../../engine/ui/widgets/ui_radio_button.h).

<a id="selection-controls-uicheckbox"></a>
### UiCheckbox

`UiCheckboxState` is `Unchecked`/`Checked`/`Indeterminate`. `set_state` writes full state; `set_checked`/`is_checked`/`is_indeterminate` are shortcuts. `toggle` changes state; `set_on_toggled` receives final value. User/`toggle` changes notify; `set_state`/`set_checked` synchronize silently. `toggle` alternates `Checked`/`Unchecked` and maps `Indeterminate`→`Checked`, not three-state cycling. Labeled controls match.

`set_mark_style` chooses Checkmark/FilledBox/RadioDot. `set_state_textures`/`clear_state_textures` skins; `set_sounds` audio. `UiCheckboxConfig` bundles textures/sounds/overrides/mark style.

<a id="selection-controls-uiradiobutton"></a>
### UiRadioButton

`set_selected`/`is_selected` access selection; select requests it and notifies `set_on_selection_changed` on changes. Implements `UiRadioItem` without clearing siblings; use `UiRadioGroup`.

`UiRadioButtonConfig`/`set_radio_button_config`/sounds/padding/base styles/overrides correspond to Checkbox. Individual callbacks have no index; group callbacks identify items.

<a id="selection-groups"></a>
## Selection Groups: UiButtonGroup and UiRadioGroup

Groups organize buttons/radio items with at most one selection for menus/exclusive settings. Callers read results.

Headers: [ui_button_group.h](../../../../engine/ui/containers/ui_button_group.h), [ui_radio_group.h](../../../../engine/ui/containers/ui_radio_group.h).

Both inherit `UiListContainer` with at most one selection:

- selected_index returns std::optional<std::size_t>;
- `set_selected_index` returns `true` on success, `false` invalid;
- `UiRadioGroup` notifies optional indices on actual changes; `UiButtonGroup` queries only without group change callback.

`UiButtonGroup` uses `add_button(std::unique_ptr<UiButton>)` and defaults to `auto_select_first`, configurable through `set_auto_select_first`. Selection updates before original click callbacks, so queries there see new indices. Programmatic selection changes roles/state without clicks. See [header](../../../../engine/ui/containers/ui_button_group.h).

Set business click callbacks before `add_button`. Later `set_on_click` replaces the wrapper containing group selection. Retain pre-insertion callbacks to run after group logic.

`UiRadioGroup` accepts `UiRadioItem` children (`UiRadioButton`/`UiLabeledRadioButton`). `set_selected_index` synchronizes members/notifies changes. Do not simultaneously force child/group states for exclusivity.

<a id="drag-value-controls"></a>
## Dragging and Values: UiDragHandle and UiSlider

Drag handles process pointers; sliders convert interaction to bounded values. Callbacks report results; callers decide use.

Headers: [ui_drag_handle.h](../../../../engine/ui/widgets/ui_drag_handle.h), [ui_slider.h](../../../../engine/ui/widgets/ui_slider.h).

<a id="drag-value-controls-uidraghandle"></a>
### UiDragHandle

`UiDragHandleConfig` bundles axis, optional bounds, overrides. `set_drag_axis` chooses Free/Horizontal/Vertical. `set_drag_bounds` limits results; `clear_drag_bounds` removes. `set_on_dragged`/`set_on_drag_ended` receive current center. `begin_drag_from_pointer`/`cancel_drag` support programmatic control.

<a id="drag-value-controls-uislider"></a>
### UiSlider

Composes a bar, drag handle, and optional number. Call `set_range` before `set_value`. A `set_step` of `std::nullopt` is continuous; otherwise values snap to a positive step. `set_orientation` chooses horizontal/vertical; `set_value_display` chooses None/Value/Percent. Value callbacks fire only on final changes.

`UiSliderConfig` atomically sets range/value/step/orientation/display/style. Formatting follows [UiNumber](text-and-values.md#value-display).

<a id="labeled-controls"></a>
## Labeled Selection Controls

These combine marks/text for settings; clicking labels activates selection.

Headers: [ui_labeled_checkbox.h](../../../../engine/ui/composites/ui_labeled_checkbox.h), [ui_labeled_radio_button.h](../../../../engine/ui/composites/ui_labeled_radio_button.h).

Construct Config or `set_labeled_*_config` for text/placement/spacing/base styles. Shared APIs: `set_label_placement`, `set_text_placement`, `set_label_spacing`, `set_typography_role`.

- LabeledCheckbox: `set_state`/`set_checked`/`toggle`/`set_on_toggled`; text clicks route to marks.
- LabeledRadioButton: `set_selected`/`is_selected`/`set_on_selection_changed`; group for exclusivity.

<a id="tabs"></a>
## Tabs and Pages: UiTabBar, UiTabView, UiTabContainer

Tabs coordinate buttons/pages. Normally use `UiTabContainer`; use separate `UiTabBar`/`UiTabView` for custom composition. Headers: [ui_tab_bar.h](../../../../engine/ui/composites/ui_tab_bar.h), [ui_tab_view.h](../../../../engine/ui/containers/ui_tab_view.h), [ui_tab_container.h](../../../../engine/ui/composites/ui_tab_container.h).

<a id="tabs-组合入口"></a>
### Composite Entry Point

`UiTabContainer`::`add_tab`(label,page) acquires ownership and returns `UiTabAddResult`; check `added` first. Nonempty `rejected_page` can return ownership on failure, but not every failure returns objects: null input gives empty/`false`. `remove_tab(index)` returns ownership, null out-of-range. Keep ownership clear after failure/removal.

<a id="tabs-单独管理按钮和页面"></a>
### Managing Buttons and Pages Separately

`UiTabBar` provides `add_tab`/`extract_tab`/`clear_tabs`, focused/selected indices and callbacks. `UiTabView` owns pages through `add_page`/`extract_page`/`clear_pages`/`set_selected_index`/`clear_selection`.

Custom composition must synchronize both `button`/page selection, not change one side only.
