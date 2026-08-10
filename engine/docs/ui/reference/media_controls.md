# 媒体控件：UiImage 与 UiAnimation

覆盖头文件：`widgets/image/ui_image.h`、`widgets/image/ui_animation.h`。

## UiImage

构造器接收借用的 `SDL_Texture*` 和 rect/position/center 几何。`set_texture` 可更换借用纹理；调用方负责纹理生命周期。`set_source_rect` 裁剪 texture space 子区域，`clear_source_rect` 恢复完整纹理。

## UiAnimation

构造时或通过 `set_animation_key` 绑定已注册 animation key；绑定失败返回 `false`，调用方应隐藏组件或显示 fallback。`set_loop` 覆盖注册动画的循环设置；`play`、`pause`、`resume`、`reset` 控制播放；`is_finished`、`is_paused` 查询状态。

透明度动画图像见 [opacity_variants](opacity_variants.md)；它们继承 UiImage 的 texture/source-rect API。
