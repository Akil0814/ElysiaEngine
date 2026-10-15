# UI 文本、数字与排版

本文说明文字内容与排版如何分工。内容决定显示什么，字体角色和对齐方式决定如何显示；文字尺寸还会影响父容器布局。

## 内容与字体

使用 `ui_text_key("path")` 指定本地化键，使用 `ui_raw_text("...")` 提供原始文字，两者都生成 `UiTextContent`。`elysia::typography::UiTypographyRole` 选择字体、字号和默认对齐，`TextHorizontalAlign` 与 `TextVerticalAlign` 控制文字位置。

## 测量与数值格式

`UiTextBlock::content_extent()` 根据当前宽度计算换行高度。父布局改变宽度后，应重新计算布局，不能继续使用旧高度。

`UiNumber` 通过 `set_value` 更新数值，使用 `set_decimal_places`、`set_suffix` 等接口设置格式，使用 `set_fixed_glyph_advance` 和 `set_target_height` 控制字形尺寸。详见[数值展示](../reference/value_display.md)。
