# UiControlFocusScopeHost

头文件：`engine/ui/focus/ui_control_focus_scope_host.h`。同时拥有 child 与焦点 scope 的容器基类。

特有 API：`set_focused_target`、`focused_target`、`focus_first_available`、`has_focusable_target`、`can_navigate`、`clear_focus_for_gamepad_scroll`、`restore_focus_after_gamepad_scroll`。使用 `set_focus_entries` 构造派生容器的方向邻居；一般场景调用者只使用 `focus_first_available` 和查询 API。
