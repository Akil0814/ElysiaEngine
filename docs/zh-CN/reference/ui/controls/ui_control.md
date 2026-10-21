# UiControl：交互控件基类

`UiControl` 在 `UiElement` 的基础上提供启用与焦点状态，并实现 `UiFocusable` 契约。头文件为 `engine/ui/core/ui_control.h`。

`set_enabled` 和 `is_enabled` 控制、查询可交互性；`set_focused` 和 `is_focused` 由焦点系统或容器驱动。派生控件通过 `on_ui_input_event` 处理确认和指针事件。

多控件的焦点互斥由焦点区域、列表、选择组或窗口维护。不要在各控件中分别设置焦点来替代导航系统。详见[输入与焦点](../concepts/input_focus_scroll.md)。
