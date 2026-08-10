# 输入、焦点与滚动

- `UiInputRouter` 将 RawInput 转为 `UiInputFrame`/`UiInputEvent`；`UiInputState` 查询 pressed、just pressed、just released。
- `UiFocusScope` 是导航边界；`UiControlFocusScopeHost` 提供 `focused_target`、`set_focused_target`、`focus_first_available` 和方向导航。
- `UiWindow::register_focus_scope` 注册窗口级 scope，`set_scope_neighbors` 设置跨区方向关系。
- `UiScrollState` 管理 axis、viewport/content size、offset、ratio 与 step；`UiScrollContainer` 暴露其面向组件的滚动 API。
- 输入回调中的指针坐标使用 layout screen space；presentation translation 使用 `presentation_to_layout_point` 转换。
