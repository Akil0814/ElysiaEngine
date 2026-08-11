# UiListContainer

头文件：`engine/ui/containers/ui_list_container.h`。沿主轴顺序排布 child 的焦点容器。

调用：`add_front`/`add_back` 添加 child；`set_direction` 选择 Vertical/Horizontal；`set_cross_align` 选择 Start/Center/End；`set_item_spacing` 设置间距。未设置 layout override 的 child 主轴尺寸来自 `content_extent()`。

适合表单、菜单、section body。固定 child slot 使用 `add_child(child, UiLayoutChildOptions{._use_size_override=true, ...})`。

## 尺寸与对齐

Vertical List 使用 child desired height 作为主轴长度；Horizontal List 使用 desired width。交叉轴默认 Center，`Start` 会将表单项贴到 content rect 起点，`End` 贴到终点。`set_padding` 来自 `UiChildHost`，会同时影响 intrinsic size 和 child 起点。

```cpp
list.set_direction(elysia::ui::UiListDirection::Vertical);
list.set_cross_align(elysia::ui::UiLayoutAlign::Start);
list.set_item_spacing(8.0f);
```
