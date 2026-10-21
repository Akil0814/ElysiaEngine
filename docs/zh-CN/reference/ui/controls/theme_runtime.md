# 样式与主题运行时

本文说明主题如何解析并应用到组件。应用代码通常只操作 `UiThemeManager`，样式合成和类型适配由内部对象完成。

相关头文件：`ui_style.h`、`ui_theme_style_resolver.h`、`ui_theme_manager.h`。

- `UiStyleState<Style>`：组件内部合成基础样式与局部字段覆盖；调用 `reset`、`set_base_style`、`set_style_overrides`、`effective_style`、`clear_style_overrides`。
- `UiThemeStyleResolver`：用 `register_adapter` 注册类型适配器，再用 `apply` 将一个元素与 `UiTheme` 解析为样式及后续遍历策略；整棵子树的遍历由 `UiThemeManager` 负责。
- `UiThemeManager`：应用代码入口。保存 `register_root` 返回的 `UiThemeRegistration`；使用 `set_theme`、`current_theme`、`reapply_theme`。整棵根子树可用 `unregister_root` 或登记对象的 `reset` 解除绑定；`detach_subtree` 用于移除子树的主题上下文。

完整覆盖替换规则见[样式专题](../concepts/style_theme.md)。

## 内置主题的背景职责

七种主题共用背景映射规则，组件按用途选择角色，无需判断主题名称：

- `UiWindow` 与 `UiPanelVisualRole::Screen` 使用页面背景。
- `UiPanelVisualRole::Default`、`List` 与 `UiChromeContainer` 主体使用分区背景；Chrome 标题底板使用页面背景。
- `UiScrollContainer` 默认背景色为透明，显示宿主窗口、分区或弹出面板的底色。滚动条和边框仍使用当前主题颜色。
- `Dialog` 角色保留独立的原有背景；下拉菜单由不透明的 `List` 面板提供底色。

| 主题 | 页面背景 | 分区背景 |
| --- | --- | --- |
| BlueGlassMoon | `#08101C` | `#0C224E` |
| ElysiaLight | `#F7DFF6` | `#FDFAFC` |
| ElysiaDark | `#141D4C` | `#2E3D7C` |
| EvangelionUnit00 | `#DCEAF6` | `#F3F6FA` |
| EvangelionUnit01 | `#221635` | `#31214C` |
| EvangelionUnit02 | `#4D1118` | `#6C181F` |
| QuietSlate | `#20262E` | `#363F4A` |

背景映射只修改上述主题背景字段，不修改文字、控件交互状态或边框颜色。需要自有底色的滚动容器可用 `UiScrollContainerStyleOverrides::background_color` 显式覆盖；覆盖在运行时切换主题后仍保留。
