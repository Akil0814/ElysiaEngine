# UiDropdown

头文件：`engine/ui/composites/ui_dropdown.h`。带 transient popup 的选择控件。

调用顺序：`set_options`/`add_option` 后 `set_selected_index`；将其纳入 Window child tree 后调用 `register_with_window(window)`；`open`、`close`、`toggle`、`is_open` 控制 popup。`set_on_selection_changed` 接收选中 index。注销使用 `unregister_from_window`。

disabled option 仍可见但不会成为键盘/手柄焦点。没有有效选项或未注册 Window 时 `open()` 不会打开。替换 `set_options` 会重建 option rows；如需保留选择，应在替换后检查并重新 `set_selected_index`。
