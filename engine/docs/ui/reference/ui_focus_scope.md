# UiFocusScope

头文件：`engine/ui/focus/ui_focus_scope.h`。焦点导航的抽象契约。

公开契约：`focus_scope_element`、`set_scope_focused`、`is_scope_focused`、`focused_target`、`focus_first_available`、`has_focusable_target`、`can_navigate`、`contains_focus_point`。一般调用者通过具体 host/container 使用，不直接实现该接口。
