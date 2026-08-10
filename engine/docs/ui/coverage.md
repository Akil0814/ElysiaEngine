# 公开头文件覆盖清单

命名规则：类参考页严格使用对应头文件 stem，例如 `ui_scroll_container.h` → `reference/ui_scroll_container.md`；专题页使用 snake_case。以下映射覆盖 `engine/ui` 所有公开头文件。

| 头文件组 | 文档 |
| --- | --- |
| `core/ui_element.h` | [UiElement](reference/ui_element.md) |
| `core/ui_child_host.h` | [UiChildHost](reference/ui_child_host.md)、[child_layout](concepts/child_layout.md) |
| `core/ui_control.h`, `ui_focusable.h`, `ui_radio_item.h`, `ui_text_align.h`, `ui_render_command_range_utils.h` | [UiControl](reference/ui_control.md)、[input_focus_scroll](concepts/input_focus_scroll.md)、[text_number](concepts/text_number.md) |
| `layout/*.h` | [child_layout](concepts/child_layout.md) |
| `input/*.h`, `input/contracts/*.h` | [input_focus_scroll](concepts/input_focus_scroll.md)、[输入与滚动运行时](reference/input_runtime.md) |
| `focus/*.h` | [UiFocusScope](reference/ui_focus_scope.md)、[UiControlFocusScopeHost](reference/ui_control_focus_scope_host.md)、[input_focus_scroll](concepts/input_focus_scroll.md) |
| `scroll/ui_scroll_state.h` | [输入与滚动运行时](reference/input_runtime.md) |
| `effects/*.h` | [effects](concepts/effects.md)、[动画基础状态机](reference/animation_primitives.md)、[透明度动画组件](reference/opacity_variants.md) |
| `text/*.h` | [text_number](concepts/text_number.md) |
| `style/*.h` | [style_theme](concepts/style_theme.md)、[样式与主题运行时](reference/theme_runtime.md) |
| `widgets/ui_button.h` | [UiButton](reference/ui_button.md) |
| `widgets/ui_checkbox.h`, `widgets/ui_radio_button.h` | [基础选择控件](reference/selection_controls.md) |
| `widgets/ui_drag_handle.h`, `widgets/ui_slider.h` | [拖拽与数值调节](reference/drag_value_controls.md) |
| `widgets/ui_bar.h`, `widgets/number/ui_number.h` | [数值展示](reference/value_display.md) |
| `widgets/ui_text_input.h` | [UiTextInput](reference/ui_text_input.md) |
| `widgets/label/ui_label.h` | [UiLabel](reference/ui_label.md) |
| `widgets/text/ui_text_block.h` | [UiTextBlock](reference/ui_text_block.md) |
| `widgets/image/ui_image.h`, `widgets/image/ui_animation.h` | [媒体控件](reference/media_controls.md) |
| `widgets/label/ui_fade_label.h`, `ui_blink_label.h`, `ui_pulse_label.h`, `widgets/image/ui_fade_image.h`, `ui_blink_image.h`, `ui_pulse_image.h` | [透明度动画组件](reference/opacity_variants.md) |
| `containers/*.h` | 对应 `reference/ui_*_container.md`、group、tab view 页面 |
| `composites/*.h` | 对应 `reference/ui_*.md` composite 页面 |
| `presets/settings_panel.h` | [SettingsPanel](reference/settings_panel.md) |
| `window/*.h` | [window_surfaces](concepts/window_surfaces.md)、[UiWindow](reference/ui_window.md) |

新增公开头文件时必须在本表增加映射；新增公开类必须增加独立参考页或明确归入现有专题页。
