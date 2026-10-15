# 数值展示控件：UiBar 与 UiNumber

`UiBar` 用填充比例表达数值，`UiNumber` 将数值排成文字。它们只呈现调用方提供的数据，不自行计算生命值、冷却或其他业务状态。

相关头文件：`widgets/ui_bar.h`、`widgets/number/ui_number.h`。

## UiBar

`set_range(min, max)` 加 `set_value(value)` 将数值映射为填充比例；`set_ratio(ratio)` 直接设定 `[0, 1]` 范围的填充比例。`min_value`、`max_value`、`value`、`ratio` 查询最终状态。`set_fill_direction` 选择四个填充方向，`set_padding` 留出可绘制内边距。

## UiNumber

`set_value(double)` 更新数字；`set_decimal_places`、`set_trim_trailing_zeros`、`set_keep_decimal_point`、`set_suffix` 管理文本格式。`set_digit_spacing`、`set_fixed_glyph_advance`、`set_target_height` 管理字形几何；对应 `clear_*` 移除可选约束。

`UiNumber` 按字符复用本地化纹理缓存，并使用公共字形序列排版算法生成渲染命令。
