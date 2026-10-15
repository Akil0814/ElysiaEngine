# UiDropdown：下拉选择

头文件：`engine/ui/composites/ui_dropdown.h`。通过临时弹出列表提供选项选择。

## 配置与打开

`set_options`/`add_option` 后 `set_selected_index`；将其纳入窗口子树后调用 `register_with_window(window)`；`open`、`close`、`toggle`、`is_open` 控制弹出列表。`set_on_selection_changed` 接收选中索引。注销使用 `unregister_from_window`。

禁用选项仍然显示但不会成为键盘/手柄焦点。没有有效选项或未注册窗口时 `open()` 不会打开。`set_options` 会重建选项行，并尝试保留原索引；原索引越界或对应新选项禁用时，改选首个可用项，没有可用项时清空选择并关闭列表。保留的是索引，不是选项内容的业务身份。`set_options` 本身不发送选择变化回调；`set_selected_index` 在合法索引实际变化时会触发回调，并非所有程序设置都是静默操作。
