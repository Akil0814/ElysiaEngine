# UiElement：UI 元素基类

`UiElement` 是可渲染 UI 对象的基类，提供几何、状态和绘制指令提交能力。头文件为 `engine/ui/core/ui_element.h`。

构造时可以提供矩形、位置与尺寸，或中心与尺寸。共同的几何和生命周期接口见[元素专题](../concepts/element_lifecycle.md)。自定义元素重写 `submit_ui_render_commands` 和 `content_extent`，分别提供绘制内容和布局测量结果；标准场景负责调用绘制提交接口。

`screen_rect` 表示布局矩形，`presentation_screen_rect` 还包含祖先的呈现位移。需要与实际显示位置比较时，应先确认使用哪一种坐标。
