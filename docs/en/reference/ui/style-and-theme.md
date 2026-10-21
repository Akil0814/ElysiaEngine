# Styles, Themes, and Visual Roles

Base styles supply complete values, overrides fix selected fields, roles express purpose. Theme managers `apply` these to registered subtrees.

See [UI guide](../../user-guide/ui/README.md#create-window) and [index](README.md#browse-by-class).

<a id="style-contract"></a>
## Style Overrides and Theme Lifetime

Styles determine presentation; themes coordinate base styles. Overrides represent explicit differences preserved across `theme` changes.

<a id="style-contract-基础样式与局部覆盖"></a>
### Base Styles and Local Overrides

`set_base_style` replaces complete bases; `set_style_overrides` replaces sparse fields without merging old overrides. `style` returns effective results, `style_overrides` current overrides, `has_style_overrides` presence; `clear_style_overrides` clears.

`set_visual_role` chooses semantic purpose such as primary actions. Themes interpret roles, which hold no colors.

<a id="style-contract-主题注册的生命周期"></a>
### Theme Registration Lifetime

`UiThemeManager::register_root` returns `UiThemeRegistration`; retain it. Repeated same-root registrations share a count; last release detaches. registered queries validity, reset releases this registration, `unregister_root` detaches all for that root. Registering through another manager invalidates old registrations. `set_theme` switches/`reapply_theme` recomputes.

`UiThemeDefaults` stores process construction defaults; changes affect subsequently created/explicitly reset controls, not existing UI. Managers snapshot defaults at construction and independently manage roots.

Default is `UiBuiltinTheme::BlueGlassMoon`. `UiThemeDefaults::set_builtin_theme` returns `true` for valid themes; invalid enums return `false`/preserve defaults. `current_builtin_theme` queries enum; `theme` corresponding data. See [ui_theme_defaults.h](../../../../engine/ui/style/ui_theme_defaults.h).

Fields/roles live under `engine/ui/style/`. Component pages describe settings; [runtime](#theme-runtime) explains composition/traversal.

<a id="theme-example"></a>
## Retaining Theme Registration

This assumes live manager/window/button and headers. Hosts retain registrations beyond initialization.

```cpp
// Retain registration until the interface no longer needs theme updates.
elysia::ui::UiThemeRegistration registration = theme_manager.register_root(window);
theme_manager.set_theme(elysia::ui::UiBuiltinTheme::ElysiaDark);

elysia::ui::UiButtonStyleOverrides overrides{};
overrides.chrome.corner_radius = 10.0f;
button.set_style_overrides(overrides);
```

<a id="theme-runtime"></a>
## Theme and Style Extension Interfaces

- UiStyleState<Style> composes bases/overrides through reset, `set_base_style`, `set_style_overrides`, `effective_style`, `clear_style_overrides`.
- `UiThemeStyleResolver` `register_adapter`/`apply` resolves one element/`theme` into `style`/traversal policy; manager traverses whole subtrees.
- `UiThemeManager` is the application entry: retain registrations, `set_theme`/`current_theme`/`reapply_theme`. `unregister_root` or registration reset detaches roots; `detach_subtree` removes `theme` context.

See [UI architecture](../../architecture/subsystems/ui/README.md#theme-flow) for traversal/composite ownership/adapters.

<a id="theme-backgrounds"></a>
## Built-In Theme Background Responsibilities

Seven themes share role-based mappings without `theme`-name checks:

- `UiWindow`/`UiPanelVisualRole::Screen` use page backgrounds.
- Default/`List` panels and Chrome body use section backgrounds; Chrome header uses page backgrounds.
- Scroll backgrounds default transparent, showing host/window/section/popup color. Bars/borders retain `theme` colors.
- `Dialog` retains its own background; dropdowns use opaque `List` panels.

| Theme | Page | Section |
| --- | --- | --- |
| BlueGlassMoon | `#08101C` | `#0C224E` |
| ElysiaLight | `#F7DFF6` | `#FDFAFC` |
| ElysiaDark | `#141D4C` | `#2E3D7C` |
| EvangelionUnit00 | `#DCEAF6` | `#F3F6FA` |
| EvangelionUnit01 | `#221635` | `#31214C` |
| EvangelionUnit02 | `#4D1118` | `#6C181F` |
| QuietSlate | `#20262E` | `#363F4A` |

Mappings modify only these backgrounds, not text/interaction/borders. Explicit `UiScrollContainerStyleOverrides::background_color` persists through runtime `theme` changes.
