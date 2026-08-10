# 透明度动画组件

覆盖头文件：`ui_fade_label.h`、`ui_blink_label.h`、`ui_pulse_label.h`、`ui_fade_image.h`、`ui_blink_image.h`、`ui_pulse_image.h`。

三组 Label/Image 分别继承 `UiLabel`/`UiImage` 的全部公开 API，并额外提供相同流程：

1. `configure_playback(mode, ...)` 设置 Fade/Blink/Pulse 参数；
2. 可选 `set_on_end(callback)` 接收一次完成通知；
3. `play()` 重置并开始；
4. host 每帧调用 `update(delta)`，组件将 core opacity 写回自身。

`Fade` 适合单次进出，`Blink` 用 visible/hidden duration，`Pulse` 在最小/最大 alpha 间往返。重复播放会重置“已通知完成”状态。
