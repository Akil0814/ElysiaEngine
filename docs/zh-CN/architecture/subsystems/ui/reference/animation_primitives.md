# UI 动画基础状态机

本文面向编写自定义 UI 元素的开发者。基础状态机只计算位移或透明度，不绘制，也不会自动推进；普通界面优先使用已有的动画 Label 或 Image 组件。

相关头文件为 `ui_translation_animation_player.h`、`ui_opacity_blink_core.h`、`ui_opacity_fade_core.h` 和 `ui_opacity_pulse_core.h`。

## 位移

`UiTranslationAnimationPlayer` 通过 `bind(name, animation)` 登记命名动画，使用 `play(name)` 启动、`stop` 停止、`update(delta)` 推进。`translation` 返回当前呈现偏移，供 `UiElement` 的呈现接口使用，不改写布局矩形。

## 透明度

三个透明度状态机通过 `configure_playback` 设置模式和时间，使用 `play` 开始、`update(delta)` 推进、`opacity` 读取当前值，并用 `is_finished` 查询完成状态。组合它们的元素负责将结果写入自身透明度，详见[透明度动画组件](opacity_variants.md)。
