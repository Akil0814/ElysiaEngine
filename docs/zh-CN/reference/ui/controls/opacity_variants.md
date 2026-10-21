# 透明度动画组件

这些组件将透明度状态机组合到文字或图像中，用于淡入淡出、闪烁和脉冲效果。播放推进由 UI 更新流程负责。

相关头文件：`ui_fade_label.h`、`ui_blink_label.h`、`ui_pulse_label.h`、`ui_fade_image.h`、`ui_blink_image.h`、`ui_pulse_image.h`。

三组 Label/Image 分别继承 `UiLabel`/`UiImage` 的全部公开 API，并额外提供相同流程：

1. `configure_playback(mode, ...)` 设置 Fade/Blink/Pulse 参数；
2. 可选 `set_on_end(callback)` 接收一次完成通知；
3. `play()` 重置并开始；
4. 容器每帧调用 `update(delta)`，组件将状态机计算的透明度写回自身。

`Fade` 适合单次进出，`Blink` 使用显示与隐藏的持续时间，`Pulse` 在最小与最大透明度间往返。重复播放会重置“已通知完成”状态。
