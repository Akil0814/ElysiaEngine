# UiPanel：面板容器

`UiPanel` 为子元素提供背景、边框和方向性的焦点关系。头文件为 `engine/ui/containers/ui_panel.h`。

`add_child(child, UiPanelInsertDirection)` 根据 `Up`、`Down`、`Left` 或 `Right` 建立相邻焦点。传入 `UiLayoutChildOptions` 的重载则使用锚点布局；添加对象会转移其所有权。

`UiPanelStyle` 描述样式，`UiPanelVisualRole` 选择主题角色。局部覆盖的共同语义见[样式专题](../concepts/style_theme.md)。
