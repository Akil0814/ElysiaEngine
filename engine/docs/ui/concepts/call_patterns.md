# 公开 API 调用模式

本页是组件参考页的共同约定。所有示例假设对象仍由其 owning scene 或 `UiChildHost` 存活。

## 1. 所有权与借用指针

`add_child(std::unique_ptr<UiElement>)`、`set_content(std::unique_ptr<UiElement>)`、`set_body(std::unique_ptr<UiElement>)`、`add_tab(..., std::unique_ptr<UiElement>)` 都会转移所有权。成功返回的 `UiElement*` 只是借用：当父 host `clear_children`、`extract_child`、`reset`、`destroy` 或销毁时失效。

```cpp
auto list = std::make_unique<elysia::ui::UiListContainer>(rect);
auto row = std::make_unique<elysia::ui::UiButton>(button_rect);
elysia::ui::UiButton* row_ptr = row.get();
list->add_back(std::move(row)); // list 现在拥有 row
row_ptr->set_enabled(false);    // 仅在 list 仍拥有它时有效
```

需要取回对象时使用 `extract_child`、`extract_page`、`extract_tab` 或 `release_content`；不要对借用指针调用 `delete`。

## 2. 布局、desired size 与固定 slot

`content_extent()` 是 child 向父布局报告的 desired/minimum size，特别是可换行 `UiTextBlock`、嵌套 List、Chrome body 和 Scroll content。`screen_rect()`/`size()` 是父布局分配后的实际几何。

父 List 未提供 size override 时，主轴使用 `content_extent()`；需要固定大小时显式设置 `UiLayoutChildOptions`：

```cpp
elysia::ui::UiLayoutChildOptions fixed{};
fixed._use_size_override = true;
fixed._size_override = {320.0f, 180.0f};
host.add_child(std::move(preview), fixed);
```

`set_cross_align(Start)` 适合表单和窄控件；默认 Center 适合菜单。父宽、padding、direction、cross align 或 child options 改变后，调用路径会使布局失效并在下一次 update/render 时重新测量。

## 3. 样式与主题

组件的 `set_base_style` 替换完整基准值；`set_style_overrides` 替换稀疏覆盖树，而不是与旧 overrides 增量合并。`style()` 是最终生效样式，`style_overrides()` 是当前稀疏覆盖，`clear_style_overrides()` 回到纯 base style。

```cpp
auto registration = theme_manager.register_root(window);
theme_manager.set_theme(elysia::ui::UiBuiltinTheme::ElysiaDark);

elysia::ui::UiButtonStyleOverrides overrides{};
overrides.chrome.corner_radius = 10.0f;
button.set_style_overrides(overrides);
```

`UiThemeRegistration` 必须与已注册 root 同寿命；提前销毁 registration 会解除主题绑定。`set_visual_role` 选择语义样式，不应当用硬编码颜色代替。

## 4. 回调

`set_on_<event>` 替换此前回调，不累加。回调可修改同一 UI 树的状态，但涉及销毁当前对象、切换 scene 或替换父 content 时应在当前事件结束后执行或使用更高层调度。

- `set_on_click`: 无参数。
- `set_on_selection_changed`: group/tab 回调是 `std::optional<std::size_t>`；Dropdown 是 `std::size_t`；单个 radio 回调无参数。
- `set_on_toggled`: `UiCheckboxState`。
- `set_on_value_changed`: `float`。
- TextInput 的 `std::string_view` 只在回调调用期间可用，需要保存时复制为 `std::string`。

## 5. Window 生命周期

先让 `UiWindow` 拥有 element，再调用 `register_with_window` 或 `register_overlay`。销毁 Window、显式注销、或 element 从 child tree 移除都会使组件的借用 Window 指针失效；实现会接收 `on_window_detached`。

```cpp
auto dialog = std::make_unique<elysia::ui::UiDialog>(dialog_rect);
auto* dialog_ptr = dialog.get();
window.add_child(std::move(dialog));
dialog_ptr->register_with_window(window);
dialog_ptr->open();
```

Overlay 使用 `open_overlay(element)`/`close_overlay(element)`；Dialog、Dropdown、Tooltip 使用自己的 `open`/`close` 或注册 API。不要混用两套调用入口。
