# 公开类与调用 API 覆盖矩阵

本表用于审查“公开类是否有调用文档”。每一行的目标页必须包含构造/创建方式、特有公开方法、返回值或回调语义，以及链接到公共调用模式。

| 公开类 | 权威文档 |
| --- | --- |
| `UiElement`, `UiChildHost`, `UiControl`, `UiControlFocusScopeHost`, `UiFocusScope`, `UiWindow` | [核心组件](README.md#独立组件参考) + [call_patterns](concepts/call_patterns.md) |
| `UiInputRouter`, `UiInputState`, `UiGamepadScrollSynthesizer`, `UiScrollState` | [输入与滚动运行时](reference/input_runtime.md) |
| `UiTranslationAnimationPlayer`, `UiOpacityBlinkCore`, `UiOpacityFadeCore`, `UiOpacityPulseCore` | [动画基础状态机](reference/animation_primitives.md) |
| `UiStyleState`, `UiThemeStyleResolver`, `UiThemeManager`, `UiThemeRegistration` | [样式与主题运行时](reference/theme_runtime.md) |
| `UiButton` | [UiButton](reference/ui_button.md) |
| `UiCheckbox`, `UiRadioButton` | [基础选择控件](reference/selection_controls.md) |
| `UiDragHandle`, `UiSlider` | [拖拽与数值调节](reference/drag_value_controls.md) |
| `UiBar`, `UiNumber` | [数值展示](reference/value_display.md) |
| `UiTextInput`, `UiLabel`, `UiTextBlock` | [各文本控件](README.md#独立组件参考) + [text_number](concepts/text_number.md) |
| `UiImage`, `UiAnimation` | [媒体控件](reference/media_controls.md) |
| `UiFadeImage`, `UiBlinkImage`, `UiPulseImage`, `UiFadeLabel`, `UiBlinkLabel`, `UiPulseLabel` | [透明度动画组件](reference/opacity_variants.md) |
| `UiListContainer`, `UiGridContainer`, `UiPanel`, `UiScrollContainer`, `UiChromeContainer` | [各容器页](README.md#独立组件参考) + [child_layout](concepts/child_layout.md) |
| `UiButtonGroup`, `UiRadioGroup` | [选择组](reference/selection_groups.md) |
| `UiTabBar`, `UiTabView`, `UiTabContainer` | [页签组件](reference/tabs.md) |
| `UiLabeledCheckbox`, `UiLabeledRadioButton` | [带标签选择控件](reference/labeled_controls.md) |
| `UiDropdown`, `UiTooltip` | [对应 composite 页](README.md#独立组件参考) + [window_surfaces](concepts/window_surfaces.md) |
| `UiDialog`, `UiConfirmationDialog`, `UiOverlayWindowClient`, `UiTransientPopup` | [对话框](reference/dialogs.md) + [window_surfaces](concepts/window_surfaces.md) |
| `SettingsPanel`, `SettingsWindowMode`, `SettingsWindowSize`, `SettingsPanelDraft`, `SettingsPanelOptions`, `make_settings_window_size_options` | [SettingsPanel preset](reference/settings_panel.md) |

`UiFocusable`、`UiRadioItem`、输入 receiver contracts 和 `UiDelegatedFocusMixin` 是实现/协作契约；其公开调用语义在 [input_focus_scroll](concepts/input_focus_scroll.md) 与 [child_layout](concepts/child_layout.md) 中统一说明。
