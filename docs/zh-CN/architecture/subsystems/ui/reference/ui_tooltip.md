# UiTooltip：被动提示

`UiTooltip` 在窗口层显示触发元素的补充说明，不承担交互式对话框的职责。头文件为 `engine/ui/composites/ui_tooltip.h`。

`bind_trigger` 借用触发元素，`set_content` 接管提示内容，`release_content` 归还内容所有权。使用 `set_show_delay` 设置延迟，再用 `register_with_window` 开始观察，或用 `unregister_from_window` 停止观察。

`open`、`close` 和 `is_open` 提供显式显示控制。触发元素的借用关系不延长其生命。正常使用时，提示组件与触发元素都应位于所登记窗口的存活子树中；窗口维护阶段会清除已离开子树的触发元素或提示登记。显示内容仍由提示组件独占。详见[窗口表面规则](../concepts/window_surfaces.md)。
