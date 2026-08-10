# 动画与效果

- `UiTranslationAnimationPlayer` 管理命名 translation animation：`bind`、`remove`、`clear`、`play`、`stop`、`update`。
- `UiOpacityFadeCore`、`UiOpacityBlinkCore`、`UiOpacityPulseCore` 是无渲染状态机；设置 playback 后由 label/image 派生组件在 `update` 中写入 opacity。
- `UiFadeLabel`、`UiBlinkLabel`、`UiPulseLabel` 与对应 Image 类都提供 `configure_playback`、`play`、`update`、`set_on_end`；回调在一次播放完成时触发一次。
