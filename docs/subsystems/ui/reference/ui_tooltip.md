# UiTooltip

头文件：`engine/ui/composites/ui_tooltip.h`。被动显示的 window-level hint。

调用：`bind_trigger` 借用 trigger；`set_content` 接管 content，`release_content` 归还所有权；`set_show_delay` 设置延迟；`register_with_window` 开始观察，`unregister_from_window` 停止。`open`/`close` 可显式控制，`is_open` 查询。
