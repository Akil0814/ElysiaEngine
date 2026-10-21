# 公开 API 调用模式

本页是组件参考页的共同约定。示例均为局部调用片段，假设对象仍然存活，并由所属场景或 `UiChildHost` 持有。片段中的矩形、预览内容和窗口由调用方提供；日志宏需包含 `engine/tools/logger.h`。

## 1. 所有权与借用指针

`add_child(std::unique_ptr<UiElement>)`、`set_content(std::unique_ptr<UiElement>)`、`set_body(std::unique_ptr<UiElement>)`、`add_tab(..., std::unique_ptr<UiElement>)` 都会转移所有权。成功返回的 `UiElement*` 只是借用：对象销毁后，旧指针失效。`extract_child` 只转移所有权，对象是否继续存活取决于新的所有者；不能把“离开原容器”直接等同于“已释放”。

```cpp
auto list = std::make_unique<elysia::ui::UiListContainer>(rect);
auto row = std::make_unique<elysia::ui::UiButton>(button_rect);
elysia::ui::UiButton* row_ptr = row.get();
list->add_back(std::move(row)); // list 现在拥有 row
row_ptr->set_enabled(false);    // 仅在 list 仍拥有它时有效
```

需要取回对象时使用 `extract_child`、`extract_page`、`extract_tab` 或 `release_content`；不要对借用指针调用 `delete`。

## 2. 布局测量与固定槽位

`content_extent()` 是子元素向父布局报告的所需尺寸，特别是可换行 `UiTextBlock`、嵌套列表、标题栏容器主体和滚动容器内容。`screen_rect()`/`size()` 是父布局分配后的实际几何。

父列表未提供尺寸覆盖时，主轴使用 `content_extent()`；需要固定大小时显式设置 `UiLayoutChildOptions`：

```cpp
elysia::ui::UiLayoutChildOptions fixed{};
fixed._use_size_override = true;
fixed._size_override = {320.0f, 180.0f};
host.add_child(std::move(preview), fixed);
```

`set_cross_align(Start)` 适合表单和窄控件；默认 Center 适合菜单。父宽、内边距、方向、交叉轴对齐或子元素选项改变后，调用路径会使布局失效并在下一次更新或绘制时重新测量。

## 3. 样式与主题

组件的 `set_base_style` 替换完整基准值；`set_style_overrides` 替换稀疏覆盖树，而不是与旧覆盖配置增量合并。`style()` 是最终生效样式，`style_overrides()` 是当前稀疏覆盖，`clear_style_overrides()` 回到纯基础样式。

```cpp
// 保存注册对象，直到界面不再需要响应主题变化。
elysia::ui::UiThemeRegistration registration = theme_manager.register_root(window);
theme_manager.set_theme(elysia::ui::UiBuiltinTheme::ElysiaDark);

elysia::ui::UiButtonStyleOverrides overrides{};
overrides.chrome.corner_radius = 10.0f;
button.set_style_overrides(overrides);
```

调用方应在需要主题绑定期间保存 `UiThemeRegistration`。提前销毁登记对象会解除绑定，但不会销毁根元素。`set_visual_role` 选择语义样式，不应当用硬编码颜色代替。

## 4. 回调

`set_on_<event>` 替换此前回调，不累加。回调可修改同一 UI 树的状态，但涉及销毁当前对象、切换场景或替换父内容时应在当前事件结束后执行或使用更高层调度。

- `set_on_click`: 无参数。
- `set_on_selection_changed`: 选择组或页签回调是 `std::optional<std::size_t>`；下拉控件是 `std::size_t`；单个单选按钮回调无参数。
- `set_on_toggled`: `UiCheckboxState`。
- `set_on_value_changed`: `float`。
- 文本输入控件的 `std::string_view` 只在回调调用期间可用，需要保存时复制为 `std::string`。

## 5. 窗口注册的生命周期

覆盖层、`UiDialog` 和 `UiConfirmationDialog` 必须先成为 `UiWindow` 的直接子元素，再调用注册接口；它们只在嵌套容器中时会被拒绝。下拉控件则允许位于窗口子树的嵌套容器中。注册不转移所有权。销毁窗口、显式注销、或元素从子元素树移除都会使组件的借用窗口指针失效；实现会接收 `on_window_detached`。

```cpp
auto dialog = std::make_unique<elysia::ui::UiDialog>(dialog_rect);
elysia::ui::UiDialog* dialog_ptr = dialog.get();
window.add_child(std::move(dialog));
// 只有成功注册为覆盖层后，才能按弹窗方式打开。
if (dialog_ptr->register_with_window(window))
{
    dialog_ptr->open();
}
else
{
    ELYSIA_LOG_ERROR("ui", "Dialog must be a direct child of its window.");
}
```

覆盖层使用 `open_overlay(element)`/`close_overlay(element)`；对话框、下拉控件、提示使用自己的 `open`/`close` 或注册 API。不要混用两套调用入口。
