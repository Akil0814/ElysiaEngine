# UiElement

头文件：`engine/ui/core/ui_element.h`。所有可渲染 UI 对象的基类。

公开调用：构造器接受 rect、position/size 或 center/size；几何与生命周期见[元素专题](../concepts/element_lifecycle.md)。`submit_ui_render_commands` 由框架调用；自定义元素覆盖它和 `content_extent`。

注意：`presentation_screen_rect` 包含祖先 presentation translation；普通 `screen_rect` 不包含。
