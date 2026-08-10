# SettingsPanel preset

头文件：`engine/ui/presets/settings_panel.h`。可复用的设置表单，组合窗口模式、分辨率、三路音量、语言、状态消息及 Save/Back 动作。

## 数据与选项

- `SettingsPanelDraft` 是面板当前编辑的草稿；`set_draft` 写入并同步控件，`draft` 返回借用的只读引用。
- `SettingsPanelOptions` 提供窗口尺寸和语言候选项；`set_options` 会移除无效/重复尺寸与空/重复语言，并保留草稿中仍有效但未列出的当前值。
- `make_settings_window_size_options(usable_size, current_size)` 返回不超过可用尺寸的预设，并确保包含当前窗口尺寸。
- 面板只维护草稿，不应用或持久化设置。Save 回调接收当前 `SettingsPanelDraft`，宿主负责验证、应用和保存。

## 调用顺序

```cpp
auto panel = std::make_unique<elysia::ui::SettingsPanel>(panel_rect);
auto* panel_ptr = panel.get();
panel_ptr->set_options(options);
panel_ptr->set_draft(draft);
panel_ptr->set_on_save([this](const elysia::ui::SettingsPanelDraft& next) {
    apply_settings(next);
});
window.add_child(std::move(panel));
panel_ptr->register_with_window(window);
```

面板内部两个 Dropdown 需要窗口级 popup 注册，因此必须在面板已由对应 `UiWindow` 拥有后调用 `register_with_window`；换窗口或提前移除时调用 `unregister_from_window`。析构与 `reset` 会自动解除现有窗口注册。

`set_status_message(message, is_error)` 显示宿主提供的状态文本，`clear_status_message` 隐藏它。`set_on_back` 注册返回动作；和其他 `set_on_*` API 一样，后一次调用替换前一次回调。
