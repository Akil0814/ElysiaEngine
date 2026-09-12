# UiWindow

头文件：`engine/ui/window/ui_window.h`。根 child host、窗口级焦点、overlay、popup 与 tooltip 协调器。

主要 API：

- 样式：`set_base_style`、`set_style_overrides`、`set_hover_focus_enabled`、`set_on_cancel`。
- 焦点：`register_focus_scope`、`unregister_focus_scope`、`set_scope_neighbors`、`set_focused_scope`、`focus_first_available_scope`。
- Overlay：`register_overlay`、`unregister_overlay`、`open_overlay`、`close_overlay`、`is_overlay_open`、`overlay_options`。
- Popup/tooltip：`register_transient_popup`、`unregister_transient_popup`、`activate_transient_popup`、`register_tooltip`、`unregister_tooltip`。

示例见[窗口表面专题](../concepts/window_surfaces.md)；Window 必须先拥有 dialog/overlay 元素再注册。

## 调用流程

1. 由 Scene 创建并持有 `UiWindow`。
2. 使用 `add_child`/`create_child` 构建普通 UI 树。
3. 对独立导航区域调用 `register_focus_scope`；通过 `UiFocusScopeNeighbors` 描述跨区方向。
4. 对 dialog/overlay 完成 ownership 后注册 window surface。
5. Scene 每帧将 input、update、render 交给 Window。

焦点注册跟随窗口的子树所有权：临时隐藏、停用窗口、scope 或其祖先，不会移除注册、注册顺序或跨区域邻接关系。不可用期间，scope 不参与焦点选择和导航；恢复可用后，无需重新注册即可通过现有焦点恢复流程继续导航，并保留仍有效的历史焦点。

窗口清理焦点注册时，仅移除已销毁或已脱离窗口子树的 scope，并清理对应的当前／历史焦点缓存。需要永久取消注册时，使用 `unregister_focus_scope`。

`overlay_options(element)` 返回内部 options 的借用指针；它只在 element 已注册且仍注册时有效。修改 options 后调用 `mark_layout_dirty` 的需求由调用场景决定；优先在注册前提供完整 `UiOverlayOptions`。
