# UiTextBlock

头文件：`engine/ui/widgets/text/ui_text_block.h`。可换行多行文本元素。

调用：`set_text_content`/`clear_text` 管理文本；`set_typography_role`、`set_horizontal_align`、`set_padding` 配置测量与绘制。其 `content_extent()` 使用当前宽度测量换行高度，父布局应在宽度变化后重新 layout。
