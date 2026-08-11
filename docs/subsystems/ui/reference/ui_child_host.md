# UiChildHost

头文件：`engine/ui/core/ui_child_host.h`。拥有并更新 child 的基础容器。

特有公开 API：`add_child`、`create_child`、`extract_child`、`clear_children`、`child_count`、`child_at`、`move_child`、`set_child_layout_options`、`set_padding`、`set_clip_children`、`mark_layout_dirty`、`update_layout_if_dirty`。调用规则见[子项与布局](../concepts/child_layout.md)。

`add_child` 与 `create_child` 的返回值均为借用指针；生命周期仍属于 host。

## 常用调用

```cpp
auto* list = window.create_child<elysia::ui::UiListContainer>(list_rect);
auto item = std::make_unique<elysia::ui::UiButton>(button_rect);
elysia::ui::UiElement* adopted = list->add_child(std::move(item));
if (!adopted) {
    // nullptr 代表未接管；传入的 unique_ptr 会在调用结束时销毁。
}
```

`set_child_layout_options(index, options)` 仅对现有 index 生效；越界调用不修改树。`update_layout_if_dirty` 可以在读取 child rect 前主动调用，避免等待下一帧渲染。
