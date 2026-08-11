# 元素、生命周期与几何

适用：所有 `UiElement` 派生类型。头文件：`engine/ui/core/ui_element.h`。

- `set_screen_rect`、`set_position`、`set_size`、`set_center` 写入实际布局几何；`screen_rect`、`position`、`size`、`center` 读取它。
- `content_extent()` 是向父布局报告的 desired/minimum size；`size()` 是当前已分配几何，不能作为父布局测量的替代来源。
- `set_visible`、`set_active`、`set_opacity`、`set_order` 控制显示、参与性、透明度和渲染顺序。
- `reset()` 恢复对象状态；对象由 `std::unique_ptr` 拥有时不要保留其销毁后的裸指针。
- `bind_translation_animation`、`play_translation_animation`、`stop_translation_animation`、`active_translation_animation` 管理 presentation translation；它不改变 layout rect。

```cpp
auto* button = window.create_child<elysia::ui::UiButton>(elysia::core::Rect{0, 0, 180, 44});
button->set_visible(true);
button->set_opacity(220);
```
