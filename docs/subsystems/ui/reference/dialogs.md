# Overlay 对话框：UiDialog 与 UiConfirmationDialog

覆盖头文件：`ui_dialog.h`、`ui_confirmation_dialog.h`。

共同调用顺序：Window 先拥有 dialog → `register_with_window(window, options)` → `open()`/`close()`。未注册、已注销或 Window 销毁后的 open/close 安全无操作。

- `UiDialog`：`set_title_content`、`set_body_content`、`set_action_content`；`set_body_scroll_enabled` 和 `set_header_visible` 控制阅读布局。
- `UiConfirmationDialog`：用 `UiConfirmationDialogConfig` + `set_config` 配置 title/message/confirm/cancel/close；`set_on_confirm` 注册确认动作。确认时先关闭再执行回调。

Overlay placement、modal、cancel/outside-click 规则见 [window_surfaces](../concepts/window_surfaces.md)。
