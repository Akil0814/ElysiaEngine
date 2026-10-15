# 样式、主题与视觉角色

样式决定组件如何呈现，主题提供一组协调的基础样式。局部覆盖用于表达调用方明确指定的差异；切换主题时，这些差异应继续保留。

## 基础样式与局部覆盖

`set_base_style` 替换完整基础样式，`set_style_overrides` 替换只包含部分字段的覆盖配置。后者不是与旧覆盖增量合并。`style` 返回生效结果，`style_overrides` 返回当前覆盖，`has_style_overrides` 查询是否存在覆盖；`clear_style_overrides` 清除局部配置。

`set_visual_role` 选择组件的语义角色，例如主要操作按钮。角色交给主题解释，本身不持有颜色。

## 主题注册的生命周期

`UiThemeManager::register_root` 注册一棵 UI 子树，并返回 `UiThemeRegistration`。调用方需要保存登记对象；提前销毁它会解除绑定。通过 `set_theme` 切换主题，或用 `reapply_theme` 重新计算。

`UiThemeDefaults` 保存进程级的构造默认主题。修改它只影响随后创建或主动重置的控件，不会刷新已有 UI。`UiThemeManager` 在构造时取得默认主题快照，此后独立管理注册的子树。

字段定义和视觉角色位于 `engine/ui/style/`。具体组件的样式配置见相应参考页，内部合成与遍历见[样式运行时](../reference/theme_runtime.md)。
