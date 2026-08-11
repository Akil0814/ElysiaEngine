# Window 表面、Overlay、Popup 与 Tooltip

- Overlay：`UiWindow::register_overlay` 后使用 `open_overlay`、`close_overlay`、`is_overlay_open`；`UiOverlayOptions` 定义 modal、placement、outside click 与 transition。
- Dialog/ConfirmationDialog：先 `register_with_window(window)`，再 `open`/`close`；窗口销毁或注销会调用 `on_window_detached` 清理借用指针。
- Transient popup：实现 `UiTransientPopup` 的组件提供 `popup_owner`、`is_open`、`contains_popup_point`、`on_popup_input_event` 与 popup render；Dropdown 是标准实现。
- Tooltip：设置 trigger/content 后 `register_with_window`；content 所有权属于 Tooltip，trigger 是借用指针。
