# UiChromeContainer

头文件：`engine/ui/containers/ui_chrome_container.h`。含 left/title/right header slot 与 body slot 的容器。

调用：`add_left_action`、`add_title_child`、`add_right_action` 添加 header 内容；`set_body` 接管唯一 body；对应 `clear_*` 清空。`set_header_visible`、`set_header_height`、`set_header_padding`、`set_body_padding` 配置结构。

`content_extent()` 包含 header/body desired size，放入 List 时会自动撑开。
