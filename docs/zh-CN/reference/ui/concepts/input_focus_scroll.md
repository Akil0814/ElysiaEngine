# UI 输入、焦点与滚动

本文解释界面如何获得输入、决定导航目标和维护滚动位置。标准场景已接入这些流程，游戏场景通常只需配置窗口、控件和焦点关系。

## 输入如何到达控件

`UiInputRouter` 将原始输入转换成 `UiInputFrame` 和 `UiInputEvent`。`UiInputState` 保存按住、刚按下和刚释放的状态，控件据此处理导航、确认和指针操作。

输入处理使用布局坐标。元素具有呈现位移时，需要通过 `presentation_to_layout_point` 将呈现位置转换到布局空间，避免命中测试与画面错位。

## 谁维护焦点

`UiFocusScope` 表示一个导航区域。`UiControlFocusScopeHost` 提供 `focused_target`、`set_focused_target`、`focus_first_available` 和方向导航。窗口通过 `register_focus_scope` 注册区域，再用 `set_scope_neighbors` 指定跨区域关系。

## 谁维护滚动

`UiScrollState` 计算滚动轴、视口与内容尺寸、偏移、比例和步长。普通界面使用 `UiScrollContainer` 的公开接口即可；只有实现新容器时才需要直接使用底层状态。更多输入捕获与取消规则见[输入运行时](../controls/input_runtime.md)。
