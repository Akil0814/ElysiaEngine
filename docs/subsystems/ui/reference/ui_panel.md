# UiPanel

头文件：`engine/ui/containers/ui_panel.h`。带背景/边框和定向插入焦点关系的容器。

调用：`add_child(child, UiPanelInsertDirection)` 按 Up/Down/Left/Right 建立相邻焦点；传 `UiLayoutChildOptions` 时使用 anchor 布局。样式使用 `UiPanelStyle` 与 `UiPanelVisualRole`，共享规则见[样式专题](../concepts/style_theme.md)。
