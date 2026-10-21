# UiFocusScope：焦点区域契约

`UiFocusScope` 描述一个可以参与窗口导航的区域。它不负责持有其中的控件，普通界面通常通过具体容器使用这套契约。头文件为 `engine/ui/focus/ui_focus_scope.h`。

`focus_scope_element` 提供区域元素，`set_scope_focused` 与 `is_scope_focused` 管理区域焦点。`focused_target` 查询当前目标，`focus_first_available` 选择首个可用目标，`has_focusable_target`、`can_navigate` 和 `contains_focus_point` 用于判断区域能否接受相应操作。

只有实现新的焦点容器时才需要直接实现该接口。窗口级登记见[窗口参考](ui_window.md)。
