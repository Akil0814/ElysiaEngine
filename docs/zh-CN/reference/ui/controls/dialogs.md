# 覆盖层对话框：UiDialog 与 UiConfirmationDialog

对话框用于临时显示说明或请求确认。普通窗口负责持有和调度对话框，对话框自身维护内容与打开状态。

相关头文件：`ui_dialog.h`、`ui_confirmation_dialog.h`。

共同调用顺序：将对话框作为窗口的直接子元素加入 → 检查 `register_with_window(window, options)` 返回 bool → `open()`/`close()`。位于嵌套容器中的对话框即使最终由同一窗口管理，也会注册失败。未注册、已注销或窗口销毁后的 open/close 安全无操作。

- `UiDialog`：`set_title_content`、`set_body_content`、`set_action_content`；`set_body_scroll_enabled` 和 `set_header_visible` 控制阅读布局。
- `UiConfirmationDialog`：用 `UiConfirmationDialogConfig` + `set_config` 配置标题、消息、确认、取消和关闭动作；`set_on_confirm` 注册确认动作。确认时先关闭再执行回调。

覆盖层的放置、模态、取消与外部点击规则见[窗口表面专题](../concepts/window_surfaces.md)。
