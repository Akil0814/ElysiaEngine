# UI 文本、数字与排版

本文说明文字内容与排版如何分工。内容决定显示什么，字体角色和对齐方式决定如何显示；文字尺寸还会影响父容器布局。

## 内容与字体

使用 `ui_text_key("path")` 指定本地化键，使用 `ui_raw_text("...")` 提供原始文字，两者都生成 `UiTextContent`。`elysia::typography::UiTypographyRole` 选择字体、字号和默认对齐，`TextHorizontalAlign` 与 `TextVerticalAlign` 控制文字位置。

## 测量与数值格式

`UiTextBlock::content_extent()` 根据当前宽度计算换行高度。父布局改变宽度后，应重新计算布局，不能继续使用旧高度。

`UiNumber` 通过 `set_value` 更新浮点数，通过 `set_integer_value` 精确显示有符号 64 位整数。`clear_value()` 表示缺失，显示 `--`；NaN 和正负无穷也按缺失处理。读取时先用 `has_value()` 区分缺失与真实零，精确整数使用 `integer_value()` 查询。

使用 `set_decimal_places`、`set_suffix` 等接口设置格式，使用 `set_fixed_glyph_advance` 和 `set_target_height` 控制字形尺寸。需要在指定区域内完整显示时，可开启默认关闭的 `set_shrink_to_fit(true)`，让整串数字等比缩小到 padding 后的可用宽高内。缩放继续复用已有字形纹理，不改变控件的布局占位。详见[数值展示](../controls/value_display.md)。
