# 子项所有权与布局

本文说明容器如何持有子元素，以及如何为它们分配位置和尺寸。`UiChildHost`、列表、网格、面板、滚动容器和窗口都遵循这些共同规则。

## 谁持有子元素

`add_child(std::unique_ptr<UiElement>, UiLayoutChildOptions)` 接管传入对象，`create_child<T>` 则先创建对象再交给容器。返回的 `UiElement*` 只是借用指针，不负责释放对象。

`extract_child` 将所有权移出容器；`clear_children` 销毁全部子元素。`child_at` 只提供访问入口。取出对象后，原指针是否仍有效取决于新的所有者，而不是它是否仍位于原容器。

## 如何分配尺寸

子元素通过 `content_extent()` 报告所需尺寸，父布局据此分配实际几何。需要固定布局槽位时，在 `UiLayoutChildOptions` 中启用 `_use_size_override` 并提供 `_size_override`。

`UiLayoutAnchor`、`UiLayoutPadding`、`UiLayoutMargin` 和 `UiLayoutAlign` 定义在 `engine/ui/layout/ui_layout_types.h`。列表的 `set_cross_align` 决定子元素在非主轴上的对齐方式。

修改 `set_padding`、`set_clip_children` 或 `set_child_layout_options` 会让布局失效并重新计算。不要通过不断改写子元素矩形来对抗父布局；应修改父布局使用的选项。实际调用示例见[公开 API 调用模式](call_patterns.md)。
