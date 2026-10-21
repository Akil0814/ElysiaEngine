# 窗口表面、弹出层与提示

一个窗口除了普通子元素，还需要协调对话框、下拉列表和提示。它们的所有权、输入优先级与显示方式不同，不能只按绘制层级理解。

## 覆盖层与对话框

覆盖层先通过 `UiWindow::register_overlay` 登记，再使用 `open_overlay`、`close_overlay` 和 `is_overlay_open`。`UiOverlayOptions` 配置模态行为、放置方式、外部点击和过渡。

`UiDialog` 与 `UiConfirmationDialog` 使用自己的 `register_with_window` 和 `open`／`close`。对话框必须先作为窗口的直接子元素加入，再进行注册；仅位于嵌套面板中不满足条件。`register_with_window` 返回 bool，失败时不要继续当作已打开的覆盖层。窗口销毁或注销时，通过 `on_window_detached` 清理借用关系；不要混用对话框与窗口的两套入口。

## 临时弹出层

实现 `UiTransientPopup` 的组件通过 `popup_owner`、`is_open`、`contains_popup_point`、`on_popup_input_event` 和绘制接口提供弹出层协议。`UiDropdown` 是标准实现。登记弹出层协议不代表转移组件所有权。

## 被动提示

`UiTooltip` 设置触发元素和内容后，再调用 `register_with_window`。内容归提示组件持有，触发元素只是借用。提示保持被动，不承担对话框式输入交互。具体流程见[窗口参考](../controls/ui_window.md)。
