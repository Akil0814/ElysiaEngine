# Elements, Ownership, and Layout

This page covers common tree interfaces and layout differences among panels, lists, grids, scroll containers, and header/body containers.

For integration see the [UI guide](../../user-guide/ui/README.md#create-window); find other classes in the [reference index](README.md#browse-by-class).

<a id="ui-element"></a>
## UiElement: Geometry, State, and Lifecycle

`UiElement` supplies geometry, state, and presentation. Layout `size` differs from display offset; determine which state to access. See [ui_element.h](../../../../engine/ui/core/ui_element.h).

- `set_screen_rect`, `set_position`, `set_size`, `set_center` write layout geometry; `screen_rect`, `position`, `size`, `center` read it.
- `content_extent()` reports required layout `size`; allocated `size()` is not a substitute for parent measurement.
- `set_visible`, `set_active`, `set_opacity`, `set_order` control display, participation, opacity, order.
- `reset()` restores state; never retain raw pointers after `std::unique_ptr` owners destroy objects.
- `bind_translation_animation`, `play_translation_animation`, `stop_translation_animation`, `active_translation_animation` manage presentation translation without changing layout rects.

This fragment assumes a scene-registered `window` and included headers. Pointers remain valid only while the `window` owns elements.

```cpp
elysia::ui::UiButton* button = window.create_child<elysia::ui::UiButton>(elysia::core::Rect{0, 0, 180, 44});
button->set_visible(true);
button->set_opacity(220);
```

Construct with a rectangle, `position`/`size`, or `center`/`size`. Custom elements override `submit_ui_render_commands`/`content_extent` for drawing/measurement.

`screen_rect` is layout geometry; presentation_screen_rect includes ancestor translation. Hits convert through presentation_to_layout_point. Animated display positions are not layout coordinates.

<a id="ownership"></a>
## Subtree Ownership and Measurement

Containers own children and allocate geometry. `UiChildHost`, lists, grids, panels, scroll containers, and windows share these rules.

<a id="ownership-谁持有子元素"></a>
### Who Owns Children

`add_child(std::unique_ptr<UiElement>, UiLayoutChildOptions)` acquires ownership. `create_child<T>` creates then transfers. Returned `UiElement*` is borrowed and never releases.

`extract_child` transfers ownership out; `clear_children` destroys all. `child_at` is access only. After extraction, validity depends on the new owner, not original membership.

<a id="ownership-如何分配尺寸"></a>
### How Size Is Allocated

Children report `content_extent`; parents allocate geometry. For fixed slots, enable `_use_size_override` and provide `_size_override` in `UiLayoutChildOptions`.

`UiLayoutAnchor`, `UiLayoutPadding`, `UiLayoutMargin`, and `UiLayoutAlign` are in [ui_layout_types.h](../../../../engine/ui/layout/ui_layout_types.h). Lists' `set_cross_align` sets non-main-axis alignment.

`set_padding`/`set_child_layout_options` invalidate layout. `set_clip_children` changes clipping only without rebuild. Modify parent options rather than repeatedly fighting layout with child `rect` writes.

<a id="ui-child-host"></a>
## UiChildHost: Container Operations

[`UiChildHost`](../../../../engine/ui/core/ui_child_host.h) provides `add_child`, `create_child`, `extract_child`, `clear_children`, `child_count`, `child_at`, `move_child`, `set_child_layout_options`, `set_padding`, `set_clip_children`, `mark_layout_dirty`, `update_layout_if_dirty`.

`set_child_layout_options` ignores out-of-range indices. Call `update_layout_if_dirty` before reading newly changed rects if needed. `content_extent` measures; `screen_rect`/`size` are allocation, not interchangeable.

`child_at`/`child_layout_options`/`extract_child` return null for out-of-range. `move_child` returns `false` out-of-range, `true` for identical valid indices or successful moves.

`set_content`/`set_body`/`add_tab` also acquire pointers. This fragment assumes list/button/smart-pointer headers and caller-supplied `rect`/`button_rect`:

```cpp
auto list = std::make_unique<elysia::ui::UiListContainer>(rect);
auto row = std::make_unique<elysia::ui::UiButton>(button_rect);
elysia::ui::UiButton* row_ptr = row.get();
list->add_back(std::move(row)); // list now owns row
row_ptr->set_enabled(false);    // Valid only while list owns it
```

Reclaim through `extract_child`/`extract_page`/`extract_tab`/`release_content`, never `delete` borrowed pointers.

<a id="fixed-layout"></a>
### Layout Options and Fixed Slots

`UiLayoutChildOptions` fields:

- `_anchor`: nine-`position` anchor for `UiPanel`/base anchor layout.
- `_margin`: extra offset/space around children.
- `_size_override` + `_use_size_override`: explicit slot `size`; enabled only by the boolean, otherwise use `content_extent`.
- `_cross_align` / `_use_custom_cross_align` / `_fill_cross_axis`: cross-axis positioning/stretch for main-axis containers.
- Container `UiLayoutPadding` subtracts `content_rect` from `screen_rect` before layout algorithms.

Without overrides lists use `content_extent` on their main axis. Here `host` exists and `preview` is unique_ptr<UiElement> awaiting transfer; explicit override reserves a fixed slot:

```cpp
elysia::ui::UiLayoutChildOptions fixed{};
fixed._use_size_override = true;
fixed._size_override = {320.0f, 180.0f};
host.add_child(std::move(preview), fixed);
```

`set_cross_align(Start)` suits forms/narrow controls; default Center suits menus. Parent width, padding, direction, cross-alignment, and child-option changes invalidate for remeasurement on next update/draw.

<a id="ui-panel"></a>
## UiPanel: Panel Container

`UiPanel` provides background, border, and directional focus. Header: [ui_panel.h](../../../../engine/ui/containers/ui_panel.h).

`add_child`(child,UiPanelInsertDirection) establishes Up/`Down`/`Left`/`Right` adjacency; the `UiLayoutChildOptions` overload uses anchors. Addition transfers ownership.

`UiPanelStyle` describes style; `UiPanelVisualRole` selects theme role. See [style contracts](style-and-theme.md#style-contract) for overrides.

<a id="ui-list-container"></a>
## UiListContainer: List Container

Header: [ui_list_container.h](../../../../engine/ui/containers/ui_list_container.h). Main-axis layout/focus suits menus/forms.

<a id="ui-list-container-基本配置"></a>
### Basic Configuration

`add_front`/`add_back` inserts; `set_direction` chooses Vertical/Horizontal; `set_cross_align` `Start`/Center/`End`; `set_item_spacing` spacing. Without overrides, main-axis sizes come from `content_extent`.

Lists also group body text. Pass [layout overrides](#fixed-layout) for fixed slots.

<a id="ui-list-container-尺寸与对齐"></a>
### Size and Alignment

Vertical lists use required heights; horizontal required widths. Cross-axis defaults Center; `Start` aligns at content origin, `End` at its end. Inherited `set_padding` affects measurement and child origin.

This configures an existing `UiListContainer list`:

```cpp
list.set_direction(elysia::ui::UiListDirection::Vertical);
list.set_cross_align(elysia::ui::UiLayoutAlign::Start);
list.set_item_spacing(8.0f);
```

<a id="ui-grid-container"></a>
## UiGridContainer: Grid Container

`UiGridContainer` lays out/focuses grids for inventories/multicolumn selection. Header: [ui_grid_container.h](../../../../engine/ui/containers/ui_grid_container.h).

`add_child` acquires; `set_column_count`, `set_cell_spacing`, `set_fill_by_row` configure. Returned pointers are access only.

`content_extent` returns the larger of explicit/required content `size`. See [ownership/layout](#ownership).

<a id="ui-scroll-container"></a>
## UiScrollContainer: Scroll Container

Header: [ui_scroll_container.h](../../../../engine/ui/containers/ui_scroll_container.h). A scrollable viewport for one content child, suited to long lists/text.

<a id="ui-scroll-container-内容与滚动"></a>
### Content and Scrolling

`set_content` replaces/acquires; `clear_content` removes. `set_scroll_axis`, `set_scrollbar_visibility`, `set_scroll_offset`, `set_scroll_step` and x/y variants configure. `scroll_offset`, `max_scroll_offset`, `viewport_rect`, `content_rect` query.

Content `content_extent` determines scroll range; hardcoded large heights do not replace measurement.

<a id="ui-scroll-container-行为细节"></a>
### Behavior Details

`UiScrollAxis::Auto` resolves overflow; `UiScrollBarVisibility::Auto` show overflowing axes only. `Hidden` hides bars without disabling scrolling. `set_scroll_offset` clamps to legal `max_scroll_offset`, accepting out-of-range safely.

Replacing content destroys the old object. To retain it, `extract_child(0)` or transfer ownership first.

<a id="ui-chrome-container"></a>
## UiChromeContainer: Header and Body

`UiChromeContainer` combines a fixed header with one body slot. Header has left actions, title, right actions. Header: [ui_chrome_container.h](../../../../engine/ui/containers/ui_chrome_container.h).

`add_left_action`/`add_title_child`/`add_right_action` populate header; `set_body` acquires body; matching `clear_*` removes. `set_header_visible`, `set_header_height`, `set_header_padding` and `set_body_padding` configure structure.

`content_extent` includes header/body requirements for parent lists. Replacing body affects old lifetime; see [ownership](#ownership).
