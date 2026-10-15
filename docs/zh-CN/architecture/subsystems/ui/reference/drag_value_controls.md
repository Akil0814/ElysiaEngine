# 拖拽与数值调节：UiDragHandle 与 UiSlider

拖拽柄处理指针移动，滑条则将交互转换为范围内的数值。两者的回调反映交互结果，业务如何使用数值由调用方决定。

相关头文件：`widgets/ui_drag_handle.h`、`widgets/ui_slider.h`。

## UiDragHandle

`UiDragHandleConfig` 可一次设置拖拽轴、可选边界和局部样式覆盖。`set_drag_axis` 选择 Free/Horizontal/Vertical；`set_drag_bounds` 会将拖拽结果限制在边界内；`clear_drag_bounds` 允许无边界拖拽。`set_on_dragged` 与 `set_on_drag_ended` 接收当前中心。`begin_drag_from_pointer`/`cancel_drag` 适合程序化控制。

## UiSlider

滑条内部组合数值条、拖拽柄和可选的数字显示。先 `set_range`，再 `set_value`；`set_step` 为 `nullopt` 时连续变化，否则吸附到正步长。`set_orientation` 选择横/竖，`set_value_display` 选择 None/Value/Percent，`set_on_value_changed` 只在最终值实际改变时触发。

`UiSliderConfig` 可原子设置范围、初值、步长、方向、显示方式与样式。数值格式 API 与 [数值展示](value_display.md) 的 `UiNumber` 规则对应。
