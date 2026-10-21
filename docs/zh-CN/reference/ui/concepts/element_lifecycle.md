# 元素、生命周期与几何

本文说明所有 `UiElement` 派生类型共同的几何、状态和呈现接口。头文件为 `engine/ui/core/ui_element.h`。布局尺寸与实际显示偏移是不同概念，使用前应先确认需要读写哪一种状态。

- `set_screen_rect`、`set_position`、`set_size`、`set_center` 写入实际布局几何；`screen_rect`、`position`、`size`、`center` 读取它。
- `content_extent()` 是向父布局报告的所需的布局尺寸；`size()` 是当前已分配几何，不能作为父布局测量的替代来源。
- `set_visible`、`set_active`、`set_opacity`、`set_order` 控制显示、参与性、透明度和渲染顺序。
- `reset()` 恢复对象状态；对象由 `std::unique_ptr` 拥有时不要保留其销毁后的裸指针。
- `bind_translation_animation`、`play_translation_animation`、`stop_translation_animation`、`active_translation_animation` 管理呈现位移；它不改变布局矩形。

以下局部片段假设 `window` 是已登记到场景的窗口，控件头文件已包含；返回指针只在窗口拥有该元素时可用。

```cpp
elysia::ui::UiButton* button = window.create_child<elysia::ui::UiButton>(elysia::core::Rect{0, 0, 180, 44});
button->set_visible(true);
button->set_opacity(220);
```
