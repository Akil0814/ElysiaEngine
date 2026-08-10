# Elysia Engine UI API 参考

本目录以 `engine/ui/**/*.h` 为唯一事实来源。公开 API 由独立组件页或同类组件专题页覆盖；跨组件的重复调用只在专题页解释一次。

## 入口

- 课程论文：[English](elysia_ui_framework_design_and_implementation.en.md) / [简体中文](elysia_ui_framework_design_and_implementation.zh-CN.md)
- [Engine/UI 学习型架构详解](engine_ui_learning_guide.md)（面向初学者：从一帧流程到容器、焦点、主题和真实场景）
- [公开头文件覆盖清单](coverage.md)
- [公开类与调用 API 覆盖矩阵](api_coverage.md)
- [公共元素、生命周期与几何](concepts/element_lifecycle.md)
- [子项所有权与布局](concepts/child_layout.md)
- [输入、焦点与滚动](concepts/input_focus_scroll.md)
- [样式、主题与视觉角色](concepts/style_theme.md)
- [文本、数字与排版](concepts/text_number.md)
- [窗口表面与注册](concepts/window_surfaces.md)
- [动画与效果](concepts/effects.md)
- [公开 API 调用模式](concepts/call_patterns.md)

## 独立组件参考

- Core 与运行时：[UiElement](reference/ui_element.md)、[UiChildHost](reference/ui_child_host.md)、[UiControl](reference/ui_control.md)、[UiControlFocusScopeHost](reference/ui_control_focus_scope_host.md)、[UiWindow](reference/ui_window.md)、[输入与滚动运行时](reference/input_runtime.md)、[动画基础状态机](reference/animation_primitives.md)、[样式与主题运行时](reference/theme_runtime.md)
- Widgets：[Button](reference/ui_button.md)、[基础选择控件](reference/selection_controls.md)、[拖拽与数值调节](reference/drag_value_controls.md)、[TextInput](reference/ui_text_input.md)、[数值展示](reference/value_display.md)、[Label](reference/ui_label.md)、[TextBlock](reference/ui_text_block.md)、[媒体控件](reference/media_controls.md)、[透明度动画组件](reference/opacity_variants.md)
- Containers：[List](reference/ui_list_container.md)、[Grid](reference/ui_grid_container.md)、[Panel](reference/ui_panel.md)、[ScrollContainer](reference/ui_scroll_container.md)、[ChromeContainer](reference/ui_chrome_container.md)、[选择组](reference/selection_groups.md)、[页签组件](reference/tabs.md)
- Composites：[Dropdown](reference/ui_dropdown.md)、[对话框](reference/dialogs.md)、[Tooltip](reference/ui_tooltip.md)、[带标签选择控件](reference/labeled_controls.md)
- Presets：[SettingsPanel](reference/settings_panel.md)

组件页只描述特有接口；继承的几何、样式、所有权、输入和窗口注册规则请使用上述专题页。
