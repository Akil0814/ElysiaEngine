# SettingsPanel preset

头文件：`engine/ui/presets/settings_panel.h`。可复用的设置表单，组合窗口模式、分辨率、帧率上限、VSync、三路音量、语言、状态消息及 Save/Back 动作。

面板由固定标题、单个纵向滚动内容区、固定状态消息和 Save/Back 组成。字段按 Display、Audio、General 的固定顺序排列；标题、状态和操作按钮不随内容滚动。输入映射不属于当前预设，未来应由设置场景作为独立二级页面管理。

## 组件可见性

`SettingsPanelVisibility` 控制是否创建各个内置字段，所有开关默认为 `true`。可见性在面板生命周期内不变；如果项目要更换组件集合，应重建面板。

```cpp
elysia::ui::SettingsPanelVisibility visibility{};
visibility.target_fps = false;
visibility.vsync = false;

auto panel = std::make_unique<elysia::ui::SettingsPanel>(
    panel_rect,
    visibility);
```

- 隐藏字段不创建控件，不参与布局、焦点导航或 Dropdown popup 注册。
- VSync 重启提示只在 VSync 字段可见时创建。
- 只有一个非空区段时省略区段标题；有两个或以上非空区段时，每个非空区段都显示标题。
- 所有字段都隐藏时，滚动内容为空，Save/Back 仍可用，默认焦点从 Save 开始。
- 可见性只决定是否创建编辑控件，不会从 `SettingsPanelDraft` 删除字段。隐藏字段保留宿主写入的基线值，Save 仍一次提交完整草稿。

`SettingsScenePayload::visibility` 使项目可在进入内置设置场景时选择组件。仅设置 `return_route` 的旧调用保持全量显示；缓存场景再次进入且可见性变化时，场景会重建 UI。

## 数据与选项

- `SettingsPanelDraft` 是面板当前编辑的完整草稿；`set_draft` 写入并同步已创建的控件，`draft` 返回借用的只读引用。
- `SettingsPanelOptions` 提供窗口尺寸、帧率和语言候选项；`set_options` 会移除无效或重复值，并保留草稿中仍有效但未列出的当前值。
- `make_settings_window_size_options(usable_size, current_size)` 返回不超过可用尺寸的预设，并确保包含当前窗口尺寸。
- `make_settings_target_fps_options(current_fps)` 返回 `30/60/120/240`，并确保包含当前合法帧率；144 FPS 不属于内置预设，但已有的 144 或其他自定义值不会丢失。
- `reset_navigation_state` 只将唯一滚动容器归顶，不修改草稿、候选项、可见性或回调。
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

已创建的窗口模式、帧率和语言 Dropdown 需要窗口级 popup 注册，因此必须在面板已由对应 `UiWindow` 拥有后调用 `register_with_window`；换窗口或提前移除时调用 `unregister_from_window`。隐藏的 Dropdown 不会注册。析构与 `reset` 会自动解除现有窗口注册。

`set_status_message(message, is_error)` 显示宿主提供的状态文本，`clear_status_message` 隐藏它。`set_on_back` 注册返回动作；和其他 `set_on_*` API 一样，后一次调用替换前一次回调。
