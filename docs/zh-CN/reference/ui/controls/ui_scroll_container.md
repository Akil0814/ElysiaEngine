# UiScrollContainer：滚动容器

头文件：`engine/ui/containers/ui_scroll_container.h`。为一个内容子元素提供可滚动视口，适合长列表和说明正文。

## 内容与滚动

`set_content` 替换并接管内容；`clear_content` 移除。`set_scroll_axis`、`set_scrollbar_visibility`、`set_scroll_offset`、`set_scroll_step` 及 x/y 变体控制滚动。`scroll_offset`、`max_scroll_offset`、`viewport_rect`、`content_rect` 查询结果。

内容元素的 `content_extent()` 决定可滚动范围；不要把固定大高度当作内容测量替代品。

## 行为细节

`UiScrollAxis::Auto` 会根据内容是否超出视口决定可滚动轴。`UiScrollBarVisibility::Auto` 仅在对应轴有溢出内容时显示；`Hidden` 不显示滚动条但不禁用滚动。`set_scroll_offset` 会限制在合法范围内，最大值为 `max_scroll_offset`，所以可安全传入越界值。

`set_content` 会替换旧内容元素并销毁它；若需保留旧对象，先调用 `extract_child(0)` 或在替换前转移所有权。
