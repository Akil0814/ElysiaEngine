# UiListContainer：列表容器

头文件：`engine/ui/containers/ui_list_container.h`。沿主轴排列子元素并提供焦点导航，适合菜单和表单。

## 基本配置

`add_front`/`add_back` 添加子元素；`set_direction` 选择 Vertical/Horizontal；`set_cross_align` 选择 Start/Center/End；`set_item_spacing` 设置间距。未设置尺寸覆盖的子元素主轴尺寸来自 `content_extent()`。

也可以用于分组正文。固定子元素槽位使用 `add_child(child, UiLayoutChildOptions{._use_size_override=true, ...})`。

## 尺寸与对齐

纵向列表使用子元素所需高度作为主轴长度；横向列表使用所需宽度。交叉轴默认 Center，`Start` 会将表单项贴到内容矩形起点，`End` 贴到终点。`set_padding` 来自 `UiChildHost`，会同时影响内容测量尺寸和子元素起点。

```cpp
list.set_direction(elysia::ui::UiListDirection::Vertical);
list.set_cross_align(elysia::ui::UiLayoutAlign::Start);
list.set_item_spacing(8.0f);
```
