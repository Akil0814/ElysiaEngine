# UI 动画与效果

UI 动画推进呈现状态，例如位置偏移和透明度。它们不替代布局，也不自动承担游戏规则中的计时或状态变化。

## 位移动画

`UiTranslationAnimationPlayer` 管理命名的位移动画。通过 `bind` 登记动画，使用 `play` 和 `stop` 控制播放，由 `update` 推进；`remove` 和 `clear` 移除登记。

## 透明度动画

`UiOpacityFadeCore`、`UiOpacityBlinkCore` 和 `UiOpacityPulseCore` 只计算透明度，不负责绘制。对应的 Label 和 Image 组件在 `update` 中推进状态机，再将结果写回自身透明度。

通常直接使用 `UiFadeLabel`、`UiBlinkLabel`、`UiPulseLabel` 或对应 Image 类型。它们提供 `configure_playback`、`play`、`update` 和 `set_on_end`；一次播放完成时通知一次。只有编写新的可视元素时，才需要直接组合底层状态机。详见[透明度动画组件](../reference/opacity_variants.md)。
