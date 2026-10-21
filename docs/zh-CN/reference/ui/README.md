# Elysia Engine UI API 参考

准备组合界面时，先阅读 [UI 使用指南](../../user-guide/ui/README.md)；查询某个控件时，再进入对应参考页。通用概念只在专题页集中解释，避免各页重复所有权、布局和主题规则。

本目录以 `engine/ui/` 的公开头文件、对应实现和测试为事实依据。头文件说明可调用接口，实现与测试共同解释调用结果和生命周期。公开 API 由独立组件页或同类组件专题页覆盖；跨组件的重复调用只在专题页解释一次。

## 入口

- [Engine/UI 学习型架构详解](../../architecture/subsystems/ui/engine_ui_learning_guide.md)（面向初学者：从一帧流程到容器、焦点、主题和真实场景）
- [公共元素、生命周期与几何](concepts/element_lifecycle.md)
- [子项所有权与布局](concepts/child_layout.md)
- [输入、焦点与滚动](concepts/input_focus_scroll.md)
- [样式、主题与视觉角色](concepts/style_theme.md)
- [文本、数字与排版](concepts/text_number.md)
- [窗口表面与注册](concepts/window_surfaces.md)
- [动画与效果](concepts/effects.md)
- [公开 API 调用模式](concepts/call_patterns.md)

## 独立组件参考

- Core 与运行时：[UiElement](controls/ui_element.md)、[UiChildHost](controls/ui_child_host.md)、[UiControl](controls/ui_control.md)、[UiFocusScope](controls/ui_focus_scope.md)、[UiControlFocusScopeHost](controls/ui_control_focus_scope_host.md)、[UiWindow](controls/ui_window.md)、[输入与滚动运行时](controls/input_runtime.md)、[动画基础状态机](controls/animation_primitives.md)、[样式与主题运行时](controls/theme_runtime.md)
- Widgets：[Button](controls/ui_button.md)、[HUD ActionButton](controls/ui_action_button.md)、[基础选择控件](controls/selection_controls.md)、[拖拽与数值调节](controls/drag_value_controls.md)、[TextInput](controls/ui_text_input.md)、[数值展示](controls/value_display.md)、[Label](controls/ui_label.md)、[TextBlock](controls/ui_text_block.md)、[媒体控件](controls/media_controls.md)、[透明度动画组件](controls/opacity_variants.md)
- Containers：[List](controls/ui_list_container.md)、[Grid](controls/ui_grid_container.md)、[Panel](controls/ui_panel.md)、[ScrollContainer](controls/ui_scroll_container.md)、[ChromeContainer](controls/ui_chrome_container.md)、[选择组](controls/selection_groups.md)、[页签组件](controls/tabs.md)
- Composites：[Dropdown](controls/ui_dropdown.md)、[对话框](controls/dialogs.md)、[Tooltip](controls/ui_tooltip.md)、[带标签选择控件](controls/labeled_controls.md)
- Presets：[SettingsPanel](controls/settings_panel.md)

组件页只描述特有接口；继承的几何、样式、所有权、输入和窗口注册规则请使用上述专题页。
