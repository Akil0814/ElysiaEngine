# UiControlFocusScopeHost：持有子项的焦点区域

该基类同时持有子元素并提供区域内导航，适合实现列表、网格等焦点容器。头文件为 `engine/ui/focus/ui_control_focus_scope_host.h`。

普通场景使用 `focus_first_available` 选择目标，通过 `focused_target`、`has_focusable_target` 和 `can_navigate` 查询状态。需要明确指定目标时使用 `set_focused_target`。

派生容器用 `set_focus_entries` 建立方向邻接关系；`clear_focus_for_gamepad_scroll` 与 `restore_focus_after_gamepad_scroll` 协调手柄滚动时的焦点表现。子项所有权仍遵循[容器规则](../concepts/child_layout.md)。
