# UiButton

头文件：`engine/ui/widgets/ui_button.h`。可点击的文本、图标或状态纹理控件。

调用：使用 `UiButtonConfig` 构造或 `set_button_config` 原子更新；文本可用 `set_text_content` 直接更新，图标或完整纹理集通过 `UiButtonConfig::content` 中的 `UiButtonIconContent`/`UiButtonTextureSetContent` 提供。`set_on_click` 替换点击回调，`prepend_on_click` 在现有回调前追加一个动作。`set_state_textures`、`set_sounds`、`set_padding` 配置表现。样式/role 遵循[样式专题](../concepts/style_theme.md)。

`UiButtonContent` 与纹理指针均不转移外部纹理所有权。

## 交互与状态

Button 仅在 visible、active、enabled 时响应 Confirm 和主鼠标按键。`set_enabled(false)` 会切换到 disabled 样式并阻止 click 回调。`set_focused` 通常由 FocusScope 设置；手工设置适合复合组件同步内部视觉状态，不应代替导航系统。

```cpp
button.set_text_content(elysia::ui::ui_text_key("common.confirm"));
button.set_on_click([this] { save_settings(); });
button.set_visual_role(elysia::ui::UiButtonVisualRole::Primary);
```
