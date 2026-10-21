# UiButton：按钮

头文件：`engine/ui/widgets/ui_button.h`。用于普通确认和选择操作，可显示文字、图标或状态纹理。

## 内容与回调

使用 `UiButtonConfig` 构造或 `set_button_config` 原子更新；文本可用 `set_text_content` 直接更新，图标或完整纹理集通过 `UiButtonConfig::content` 中的 `UiButtonIconContent`/`UiButtonTextureSetContent` 提供。`set_on_click` 替换点击回调，`prepend_on_click` 在现有回调前追加一个动作。

`set_state_textures`、`set_sounds`、`set_padding` 配置表现。样式与视觉角色遵循[样式专题](../concepts/style_theme.md)。

`UiButtonContent` 与纹理指针均不转移外部纹理所有权。

## 交互与状态

按钮只有在可见、活跃且启用时，才响应确认动作和主鼠标按键。`set_enabled(false)` 会切换到禁用样式并阻止点击回调。`set_focused` 通常由焦点区域设置；手工设置适合复合组件同步内部视觉状态，不应代替导航系统。

以下片段位于拥有 `button` 的宿主成员函数中，`save_settings()` 由游戏实现。

```cpp
button.set_text_content(elysia::ui::ui_text_key("common.confirm"));
// 宿主负责验证、应用和保存设置；回调不能比宿主活得更久。
button.set_on_click([this]
{
    save_settings();
});
button.set_visual_role(elysia::ui::UiButtonVisualRole::Primary);
```
