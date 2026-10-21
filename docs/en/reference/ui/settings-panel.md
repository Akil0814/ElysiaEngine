# Settings Panel

This page defines settings-form configuration. For the built-in scene, first read [settings guide](../../user-guide/builtin-scenes/settings.md).

<a id="settings-panel"></a>
## SettingsPanel: Settings Form

Header: [settings_panel.h](../../../../engine/ui/presets/settings_panel.h). Reusable window mode/resolution/FPS/VSync, three volumes, language, status, Save/Back.

The panel edits data; hosts apply/save. Below describes composition, drafts, responsibilities, and embedding.

Layout has fixed left-aligned title, one vertical scroll area, fixed status, bottom-right Back/Save. Display/Audio/General groups use theme section backgrounds without overrides; percentages sit right of sliders. Title/status/actions do not scroll. Input mapping is not part of this preset; games choose extensions.

<a id="settings-panel-组件可见性"></a>
### Component Visibility

`SettingsPanelVisibility` controls field creation, all `true` by default. Visibility is fixed during panel lifetime; rebuild to change components.

```cpp
elysia::ui::SettingsPanelVisibility visibility{};
visibility.target_fps = false;
visibility.vsync = false;

auto panel = std::make_unique<elysia::ui::SettingsPanel>(
    panel_rect,
    visibility);
```

- Hidden fields create no controls/layout/focus/popup registrations.
- VSync restart hints exist only when visible.
- `window_mode` controls both mode/size fields. Borderless disables size while retaining windowed values for return.
- One nonempty group omits its title; two or more show each nonempty group's title.
- All hidden means empty scrolling but usable Back/Save, initially focused Back.
- Visibility does not remove `draft` fields. Hidden values retain host baselines; Save submits a complete `draft`.

`SettingsScenePayload::visibility` chooses built-in fields. Old `return_route`-only calls show all. Cached reentry with changed visibility rebuilds UI.

<a id="settings-panel-数据与选项"></a>
### Data and Options

- `SettingsPanelDraft` is the complete edit state; `set_draft` synchronizes created controls, `draft` returns borrowed const data.
- `SettingsPanelOptions` supplies sizes/FPS/languages. `set_options` removes invalid/duplicate values but keeps valid current unlisted values. Optional `usable_window_size` limits sizes; oversized old values do not reappear. With visible window fields, oversized drafts choose the largest valid option, applied by hosts only on Save.
- make_settings_window_size_options(usable_size,current_size) returns fitting presets/valid custom current; if none, uses usable area. Built-in entry queries the window's display work area, falling back to primary if unknown.
- make_settings_target_fps_options returns `30/60/120/240` plus legal current FPS; 144 is not a preset but custom values persist.
- Caps may exceed refresh rates; no refresh-rate filtering.
- `reset_navigation_state` scrolls the sole container to top without changing `draft`/options/visibility/callbacks.
- Panel neither applies nor persists. Save passes `SettingsPanelDraft` to host validation/application/persistence.

<a id="settings-panel-调用顺序"></a>
### Call Order

This belongs in a host member with prepared rect/options/`draft`. Games implement `apply_settings` with application/persistence failure handling. Borrowed hosts must live until panel destruction/callback detachment.

```cpp
auto panel = std::make_unique<elysia::ui::SettingsPanel>(panel_rect);
elysia::ui::SettingsPanel* panel_ptr = panel.get();
panel_ptr->set_options(options);
panel_ptr->set_draft(draft);
// The host handles save results, then reports status to the user.
panel_ptr->set_on_save([this](const elysia::ui::SettingsPanelDraft& next)
{
    apply_settings(next);
});
window.add_child(std::move(panel));
panel_ptr->register_with_window(window);
```

Created mode/size/FPS/language dropdowns need window popup registration. Call `register_with_window` only after ownership by that `UiWindow`; call `unregister_from_window` before changing windows/removal. Hidden dropdowns do not register. Destruction/reset automatically detach.

set_status_message(message,is_error) shows host text, `clear_status_message` hides. `set_on_back` registers return, replacing previous callbacks like other `set_on_*` methods.
