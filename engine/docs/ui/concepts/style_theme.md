# 样式、主题与视觉角色

重复样式 API 的权威语义：

- `set_base_style` 替换完整基准样式；`set_style_overrides` 替换稀疏 override；`style`/`style_overrides` 读取结果；`has_style_overrides` 查询；`clear_style_overrides` 清除局部覆盖。
- `set_visual_role`/`visual_role` 选择主题语义角色，不直接拥有颜色。
- `UiThemeManager::register_root` 注册一棵 child tree，返回必须长期保存的 `UiThemeRegistration`；`set_theme` 切换 `UiBuiltinTheme`，`reapply_theme` 强制重算。
- `UiThemeDefaults` 保存进程级构造默认主题；修改它只影响随后创建或主动 reset 的控件，不会刷新既有 UI 对象。
- `UiThemeManager` 在构造时快照 `UiThemeDefaults`，之后独立管理已注册的 UI 树，不会跟随构造默认主题变化。
- `UiStyleState`、`UiThemeStyleResolver`、`UiTheme`、`UiPalette`、各组件的 `Ui*VisualRole` 及全部 style/config structs 的字段定义在 `style/` 头文件；它们由对应组件页链接说明。
