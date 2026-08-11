# 拖拽与数值调节：UiDragHandle 与 UiSlider

覆盖头文件：`widgets/ui_drag_handle.h`、`widgets/ui_slider.h`。

## UiDragHandle

`UiDragHandleConfig` 可一次设置 axis、可选 drag bounds 和 style overrides。`set_drag_axis` 选择 Free/Horizontal/Vertical；`set_drag_bounds` 会 clamp 拖拽结果；`clear_drag_bounds` 允许无边界拖拽。`set_on_dragged` 与 `set_on_drag_ended` 接收当前 center。`begin_drag_from_pointer`/`cancel_drag` 适合程序化控制。

## UiSlider

Slider 内部组合 Bar、DragHandle 和可选 Number。先 `set_range`，再 `set_value`；`set_step` 为 `nullopt` 时连续变化，否则吸附到正步长。`set_orientation` 选择横/竖，`set_value_display` 选择 None/Value/Percent，`set_on_value_changed` 只在最终值实际改变时触发。

`UiSliderConfig` 可原子设置范围、初值、step、方向、显示与 style。数值格式 API 与 [value_display](value_display.md) 的 UiNumber 规则对应。
