# 文本、数字与排版

- 使用 `ui_text_key("path")` 表示可本地化文本，`ui_raw_text("...")` 表示原始文本；类型为 `UiTextContent`。
- `elysia::typography::UiTypographyRole` 选择字体、字号与默认对齐。`TextHorizontalAlign`/`TextVerticalAlign` 控制文字位置。
- `UiTextBlock::content_extent()` 依赖当前宽度计算换行高度；父布局宽度变化后必须让其重新 layout。
- `UiNumber` 通过 `set_value`、`set_decimal_places`、`set_suffix`、`set_fixed_glyph_advance`、`set_target_height` 格式化数字。
