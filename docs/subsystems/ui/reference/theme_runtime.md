# 样式与主题运行时

覆盖头文件：`ui_style.h`、`ui_theme_style_resolver.h`、`ui_theme_manager.h`。

- `UiStyleState<Style>`：组件内部合成 base style 与 sparse overrides；调用 `reset`、`set_base_style`、`set_style_overrides`、`effective_style`、`clear_style_overrides`。
- `UiThemeStyleResolver`：用 `register_adapter` 注册类型 adapter，再用 `apply` 将一个 element 与 `UiTheme` 解析为样式及后续遍历策略；整棵子树的遍历由 `UiThemeManager` 负责。
- `UiThemeManager`：应用代码入口。保存 `register_root` 返回的 `UiThemeRegistration`；使用 `set_theme`、`current_theme`、`reapply_theme`。整棵 root 可用 `unregister_root` 或 registration 的 `reset` 解除绑定；`detach_subtree` 用于移除子树的主题上下文。

完整 override 替换规则见 [style_theme](../concepts/style_theme.md)。
