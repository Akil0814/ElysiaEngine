# 数值展示控件：UiBar 与 UiNumber

`UiBar` 用填充比例表达数值，`UiNumber` 将数值排成文字。它们只呈现调用方提供的数据，不自行计算生命值、冷却或其他业务状态。

相关头文件：`widgets/ui_bar.h`、`widgets/number/ui_number.h`。

## UiBar

`set_range(min, max)` 加 `set_value(value)` 将数值映射为填充比例；`set_ratio(ratio)` 直接设定 `[0, 1]` 范围的填充比例。`min_value`、`max_value`、`value`、`ratio` 查询最终状态。`set_fill_direction` 选择四个填充方向，`set_padding` 留出可绘制内边距。

## UiNumber

### 数值、精确整数与缺失值

`set_value(double)` 更新浮点数，`set_integer_value(std::int64_t)` 完整保留有符号 64 位整数的每一位。积分或计数可能超过 `double` 能精确表达的整数范围时，应直接使用整数接口，避免先转换为 `double`。

`integer_value()` 只在整数赋值状态返回精确值；即使浮点赋值是 `12.0`，它也返回空。`value()` 提供浮点视图，读取大整数时可能丢失精度，因此不能用它往返保存精确整数。

`clear_value()` 表示数值缺失，显示 `--`，省略小数与后缀。`set_value` 收到 NaN 或正负无穷时也进入这一状态。用 `has_value()` 区分缺失值与真实零：缺失时 `value()` 返回 `0`、`integer_value()` 返回空。再次赋予有限浮点数或整数即可恢复显示；构造和 `reset()` 后均为浮点零。

`set_decimal_places`、`set_trim_trailing_zeros`、`set_keep_decimal_point`、`set_suffix` 管理文本格式，`formatted_text()` 返回最终字符串。整数同样遵守这些设置，例如保留两位小数时 `42` 可显示为 `42.00`。百分号只追加 `%`，调用方负责把比例换算为百分数。数字始终使用 ASCII 数字和小数点，不随全局 locale 改变；当前整数接口不覆盖完整的 `uint64_t` 范围。

### 字形尺寸与区域适配

`set_digit_spacing`、`set_fixed_glyph_advance`、`set_target_height` 管理字形几何；对应 `clear_*` 移除可选约束。固定前进距离控制每个字形占用的前进空间，相邻起点之间还会加上设定字距；前进距离可以小于字形宽度并产生重叠。

`set_shrink_to_fit(true)` 把整串字形等比缩小到 `set_padding` 后的可用宽高内，字距与固定前进距离也随之缩放。它只缩小，不放大；显式设置的目标高度是缩放前的高度。对齐使用缩放后的排版范围，控件的布局占位不变。固定前进距离较大时，最后一个字形后仍保留该列的前进空间。

缩小默认关闭，`reset()` 也会恢复关闭。关闭时保留原有字形尺寸，内容可能超过控件边界。开启后，位数增加可能让整串数字变小，因此固定前进距离不能保证不同数值之间的最终屏幕列宽始终一致。

`UiNumber` 按字符复用本地化纹理缓存，并使用公共字形序列排版算法生成渲染命令。目标高度和缩放不会创建不同尺寸的字体纹理；数值变化继续复用已有数字字形，`--` 的两个字符也共享同一张减号纹理。格式化与排版仍在每次提交渲染命令时执行。
