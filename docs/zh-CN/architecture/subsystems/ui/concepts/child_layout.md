# 子项所有权与布局

适用：`UiChildHost`、`UiListContainer`、`UiGridContainer`、`UiPanel`、`UiScrollContainer`、`UiWindow`。

- `add_child(std::unique_ptr<UiElement>, UiLayoutChildOptions)` 转移所有权并返回非拥有 `UiElement*`；`create_child<T>` 创建并转移所有权。
- `extract_child` 将所有权移出；`clear_children` 销毁全部 child。`child_at` 返回借用指针。
- `UiLayoutChildOptions` 的 `_use_size_override` 是唯一固定 child slot 的方式；未设置时父布局消费 `content_extent()`。
- `UiLayoutAnchor`、`UiLayoutPadding`、`UiLayoutMargin`、`UiLayoutAlign` 见 `layout/ui_layout_types.h`。List 的 `set_cross_align` 控制非主轴位置。
- `set_padding`、`set_clip_children`、`set_child_layout_options` 会触发布局失效；不要直接修改 child 的 rect 来对抗父布局。
