# 媒体控件：UiImage 与 UiAnimation

图像控件显示已有纹理，动画控件播放已注册的动画资源。两者都负责界面呈现，不管理游戏规则。

相关头文件：`widgets/image/ui_image.h`、`widgets/image/ui_animation.h`。

## UiImage

构造器接收借用的 `SDL_Texture*` 和矩形、位置或中心对应的几何。`set_texture` 可更换借用纹理；调用方负责纹理生命周期。`set_source_rect` 裁剪纹理空间中的子区域，`clear_source_rect` 恢复完整纹理。

## UiAnimation

构造时或通过 `set_animation_key` 绑定已注册动画键；绑定失败返回 `false`，调用方应隐藏组件或显示替代内容。`set_loop` 覆盖注册动画的循环设置；`play`、`pause`、`resume`、`reset` 控制播放；`is_finished`、`is_paused` 查询状态。

透明度动画图像见 [透明度动画组件](opacity_variants.md)；它们继承 `UiImage` 的纹理与源矩形接口。
