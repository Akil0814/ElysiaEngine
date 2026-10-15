# UI 使用指南

本文按创建窗口、组合内容、配置主题和管理交互的顺序介绍常见界面。代码是需要场景上下文的局部调用片段，标准场景已负责输入、更新与绘制调度。

> 组件的完整公开调用表已迁移到 [UI API 参考入口](README.md)。本指南只保留推荐组合流程；不要再以旧的汇总 API 页作为接口依据。

## 最小窗口

`UiWindow` 是场景 UI 树的根。场景对象负责其生命周期；窗口负责子节点、焦点域、覆盖层与弹出层的协调。以下模式来自 `game/navigation/main_menu_scene.cpp`：

```cpp
auto* window = Scene::create_and_add_object<elysia::ui::UiWindow>(
    elysia::core::Rect{120, 80, 1040, 560}, 100);
window->set_padding({24.0f, 24.0f, 24.0f, 24.0f});

auto* start = window->create_child<elysia::ui::UiButton>(
    elysia::ui::UiLayoutChildOptions{
        ._anchor = elysia::ui::UiLayoutAnchor::TopLeft,
        ._margin = {0.0f, 0.0f, 0.0f, 0.0f}},
    elysia::core::Rect{0, 0, 180, 44},
    elysia::ui::UiButtonConfig{.content = elysia::ui::ui_text_key("menu_scene.demo_gallery")});
start->set_on_click([this] {
    request_scene_switch(
        example::scene_keys::ShowcaseGallery,
        example::scene::ShowcaseEnterPayload{
            .return_route = {
                .target = example::scene_keys::MainMenu,
                .reload_mode = elysia::scene::SceneReloadMode::Reuse}},
        elysia::scene::SceneReloadMode::Reuse);
});

// settings 是同一窗口中已创建的设置按钮，以下演示其回调配置。
settings->set_on_click([this] {
    request_scene_switch(
        elysia::builtin::SceneKeys::Settings,
        elysia::builtin::SettingsScenePayload{
            .return_route = elysia::scene::SceneRoute{
                .target = example::scene_keys::MainMenu,
                .reload_mode = elysia::scene::SceneReloadMode::Reuse
            }
        });
});
```

`create_child` 和 `add_child` 会转移 `unique_ptr` 所有权；返回的裸指针仅在父节点仍持有该子节点、且子节点未销毁时有效。不要把同一元素添加到两个容器，也不要自行 `delete`返回指针。

## 场景、更新、输入与渲染

场景注册的 UI 对象会参与场景更新和渲染。UI 输入应走 `UiInputRouter`：它把`RawInputFrame`/`RawInputEvent` 变成一帧状态和一组 `UiInputEvent`，然后交给根窗口。标准场景按输入顺序把事件交给 UI 接收者，普通派生场景不应再次手动分发。下面仅说明自定义输入桥接时的局部调用顺序，省略了场景级捕获和消费协调：

```cpp
elysia::ui::UiInputRouter router;
window->on_ui_input_frame(router.route_frame(raw_frame));
for (const auto& raw_event : raw_events)
    for (const auto& event : router.route_event(raw_event))
        if (window->on_ui_input_event(event)) break;

std::vector<elysia::core::UiRenderCommand> commands;
window->submit_ui_render_commands(commands);
```

高 `order` 值绘制在上方，也优先接收输入。`UiElement::screen_rect()` 是唯一的最终几何来源；不要在控件外维护另一份命中测试坐标。

## 布局和所有权

- 任何 `UiChildHost` 都可 `add_child`、`create_child`、`extract_child`、`clear_children`。
  `extract_child` 归还所有权，适合把页面或临时内容移交给别的容器。
- 使用 `set_child_layout_options` 改变单项锚点、边距、横轴对齐或尺寸覆盖；改变后容器会在
  下次更新/渲染前重新布局。
- `UiListContainer` 用 `add_back`/`add_front` 顺序堆叠，`UiGridContainer` 用列数和单元间距；
  `UiPanel` 适合在面板中按方向插入。
- `UiScrollContainer` 仅拥有一个内容节点，使用 `set_content`，而非把多项直接挂到它。

## 文本和本地化

文字通过 `UiTextContent` 表示来源：`ui_text_key("key")` 是本地化键，`ui_raw_text("text")` 是原样文本；空字符串会产生空内容。按钮、标签、对话框、下拉选项等都接受该类型。不要把资源键当作已经翻译的文本，也不要把用户输入误当作资源键。

## 主题与局部样式

注册根节点后，`UiThemeManager` 会给整棵子树应用当前主题，并会在以后添加/移除子节点时维护关联：

```cpp
elysia::ui::UiThemeManager themes;
auto registration = themes.register_root(*window); // RAII；销毁或 reset 即解绑
themes.set_theme(elysia::ui::UiBuiltinTheme::ElysiaDark);

auto overrides = start->style_overrides();
overrides.chrome.corner_radius = 8.0f;
start->set_style_overrides(overrides);
start->set_visual_role(elysia::ui::UiButtonVisualRole::Primary);
```

保留 `UiThemeRegistration` 的寿命必须不短于注册关系。样式覆盖只写需要固定的字段；`clear_style_overrides` 可重新完全接受主题。`set_base_style` 用于不受主题管理或需要显式基础样式的元素。

## 焦点、键盘和手柄

`UiListContainer`、`UiGridContainer`、`UiPanel`、`UiScrollContainer`、`UiTabView` 和`UiChromeContainer` 可作为焦点区域。将需要窗口级导航的焦点区域注册到窗口：

```cpp
window->register_focus_scope(*left, {.right = right});
window->register_focus_scope(*right, {.left = left});
window->focus_first_available_scope();
```

焦点区域内部通过 `UiControlFocusScopeHost` 维护可用控件及方向邻居。`Navigate*`、`Confirm`和 `Cancel` 是设备无关动作；鼠标可根据窗口的悬停焦点策略移动焦点。禁用、隐藏或销毁控件后，容器会在生命周期同步点修复焦点，不要保存旧的 `focused_target()` 指针。

## 滚动、弹窗与提示

- `UiScrollContainer::set_scroll_axis` 设置 `Auto`、单轴或双轴；`scroll_by`、`scroll_to_*`
  和 `ensure_visible` 可编程滚动。手柄和鼠标滚轮由容器处理，手柄滚动会协调焦点。
- `UiDialog`/`UiConfirmationDialog` 作为窗口的直接子元素后，调用自身的
  `register_with_window(window, options)` 并检查 bool，再用 `open`/`close` 管理。模态覆盖层先接收输入，关闭时窗口尝试恢复先前焦点区域。只有自定义覆盖层元素才直接调用`UiWindow::register_overlay` 与 `open_overlay`/`close_overlay`。
- `UiDropdown::register_with_window(window)` 将其展开列表注册为临时弹出层；对象
  保留列表所有权，窗口仅负责绘制层级与输入优先级。销毁或换窗口前调用`unregister_from_window`。
- `UiTooltip::set_content` 后注册到窗口；它是被动渲染层，激活的临时弹出层可以阻止其显示。

## 生命周期清单

1. 创建根窗口并在场景退出时 `destroy()` 或由场景释放。
2. 子项只由一个 `UiChildHost` 所有；临时移动先 `extract_child`。
3. RAII 保存主题注册；overlay/popup/tooltip 的注册对象和窗口不得有悬空关系。
4. `reset()` 复用元素前会清除交互与子树状态；不要假设回调、选择或弹出状态仍存在。
5. 对话框、下拉和提示的析构/解绑行为由
   `tests/ui/ui_popup_lifecycle_tests.cpp` 覆盖；主题注册行为由`tests/ui/ui_style_tests.cpp` 覆盖。
