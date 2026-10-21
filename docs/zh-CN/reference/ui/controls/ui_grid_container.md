# UiGridContainer：网格容器

`UiGridContainer` 按网格排列子元素，并为它们建立焦点关系。它适合物品格或多列选择界面。头文件为 `engine/ui/containers/ui_grid_container.h`。

通过 `add_child` 接管子元素，使用 `set_column_count` 设置列数、`set_cell_spacing` 设置间距、`set_fill_by_row` 选择填充顺序。返回的子项指针只提供访问能力。

`content_extent` 综合显式尺寸与内容所需尺寸，返回较大的结果。父布局和固定槽位规则见[子项与布局](../concepts/child_layout.md)。
