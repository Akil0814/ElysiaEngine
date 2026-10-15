# 样式与主题运行时

本文说明主题如何解析并应用到组件。应用代码通常只操作 `UiThemeManager`，样式合成和类型适配由内部对象完成。

相关头文件：`ui_style.h`、`ui_theme_style_resolver.h`、`ui_theme_manager.h`。

- `UiStyleState<Style>`：组件内部合成基础样式与局部字段覆盖；调用 `reset`、`set_base_style`、`set_style_overrides`、`effective_style`、`clear_style_overrides`。
- `UiThemeStyleResolver`：用 `register_adapter` 注册类型适配器，再用 `apply` 将一个元素与 `UiTheme` 解析为样式及后续遍历策略；整棵子树的遍历由 `UiThemeManager` 负责。
- `UiThemeManager`：应用代码入口。保存 `register_root` 返回的 `UiThemeRegistration`；使用 `set_theme`、`current_theme`、`reapply_theme`。整棵根子树可用 `unregister_root` 或登记对象的 `reset` 解除绑定；`detach_subtree` 用于移除子树的主题上下文。

完整覆盖替换规则见[样式专题](../concepts/style_theme.md)。
