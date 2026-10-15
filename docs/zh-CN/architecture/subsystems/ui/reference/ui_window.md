# UiWindow：窗口与表面协调

头文件：`engine/ui/window/ui_window.h`。作为根容器持有 UI 子树，并协调窗口级焦点、覆盖层、弹出列表与提示。

主要 API：

- 样式：`set_base_style`、`set_style_overrides`、`set_hover_focus_enabled`、`set_on_cancel`。
- 焦点：`register_focus_scope`、`unregister_focus_scope`、`set_scope_neighbors`、`set_focused_scope`、`focus_first_available_scope`。
- 覆盖层：`register_overlay`、`unregister_overlay`、`open_overlay`、`close_overlay`、`is_overlay_open`、`overlay_options`。
- 弹出层与提示：`register_transient_popup`、`unregister_transient_popup`、`activate_transient_popup`、`register_tooltip`、`unregister_tooltip`。

示例见[窗口表面专题](../concepts/window_surfaces.md)。对话框和覆盖层必须是窗口的直接子元素；焦点区域和下拉控件的弹出层可来自窗口内的嵌套子树，不能将两种登记条件混为一谈。

## 调用流程

1. 由场景创建并持有 `UiWindow`。
2. 使用 `add_child`/`create_child` 构建普通 UI 树。
3. 对独立导航区域调用 `register_focus_scope`；通过 `UiFocusScopeNeighbors` 描述跨区方向。
4. 对对话框或覆盖层完成所有权转移后注册窗口表面。
5. 场景每帧将输入、更新和绘制交给窗口。

焦点注册跟随窗口的子树所有权：临时隐藏、停用窗口、焦点区域或其祖先，不会移除注册、注册顺序或跨区域邻接关系。不可用期间，焦点区域不参与焦点选择和导航；恢复可用后，无需重新注册即可通过现有焦点恢复流程继续导航，并保留仍有效的历史焦点。

窗口清理焦点注册时，仅移除已销毁或已脱离窗口子树的焦点区域，并清理对应的当前／历史焦点缓存。需要永久取消注册时，使用 `unregister_focus_scope`。

`overlay_options(element)` 返回内部选项的借用指针；它只在元素仍处于注册状态时有效。修改选项后调用 `mark_layout_dirty` 的需求由调用场景决定；优先在注册前提供完整 `UiOverlayOptions`。
