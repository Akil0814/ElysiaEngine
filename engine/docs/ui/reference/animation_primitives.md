# 动画基础状态机

覆盖头文件：`ui_translation_animation_player.h`、`ui_opacity_blink_core.h`、`ui_opacity_fade_core.h`、`ui_opacity_pulse_core.h`。

- `UiTranslationAnimationPlayer`：`bind(name, animation)` 注册、`play(name)` 启动、`stop` 停止、`update(delta)` 推进；`translation` 是当前偏移，供 `UiElement` presentation API 使用。
- 三个 opacity core：`configure_playback` 配置模式和时间，`play` 开始，`update(delta)` 推进，`opacity` 读取当前 alpha，`is_finished` 查询完成。
- core 不渲染、不自动 update；优先使用对应 Label/Image 变体，只有自定义可视元素才直接组合 core。
