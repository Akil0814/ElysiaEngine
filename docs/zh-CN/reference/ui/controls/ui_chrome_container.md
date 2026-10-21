# UiChromeContainer：标题栏与主体容器

`UiChromeContainer` 将固定标题栏与主体内容组合起来。标题栏具有左侧操作、标题和右侧操作三个位置，主体只有一个内容槽位。头文件为 `engine/ui/containers/ui_chrome_container.h`。

通过 `add_left_action`、`add_title_child`、`add_right_action` 添加标题栏元素，使用 `set_body` 接管主体；对应的 `clear_*` 接口清空内容。`set_header_visible`、`set_header_height`、`set_header_padding` 和 `set_body_padding` 配置结构。

`content_extent()` 包含标题栏与主体所需尺寸，因此放入列表后可由父布局据此分配空间。设置主体时应留意旧对象的生命周期，详见[所有权规则](../concepts/child_layout.md)。
