# UiControl

头文件：`engine/ui/core/ui_control.h`。可交互元素基类，继承 `UiElement` 和 `UiFocusable`。

调用：`set_enabled`/`is_enabled` 控制可交互性；`set_focused`/`is_focused` 由焦点系统或容器驱动。派生控件覆盖 `on_ui_input_event` 处理 Confirm 与指针事件。

不要手工维持多个控件的焦点互斥；使用 FocusScope、List、Group 或 Window。
