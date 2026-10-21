# UiTextBlock：多行文本

`UiTextBlock` 根据可用宽度显示可换行文字，适合说明正文和长消息。头文件为 `engine/ui/widgets/text/ui_text_block.h`。

`set_text_content` 与 `clear_text` 管理内容，`set_typography_role`、`set_horizontal_align` 和 `set_padding` 配置测量与绘制。

`content_extent()` 使用当前宽度测量换行后的高度。父容器改变宽度后，需要重新计算布局，否则旧高度无法准确容纳文字。详见[文本与排版](../concepts/text_number.md)。
