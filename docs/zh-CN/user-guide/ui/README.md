# UI 入门与导航

UI 使用 UiElement 派生对象，与 GameObject 分开管理。通常先创建场景根窗口，再将按钮、列表等放入窗口或容器。详细操作继续使用现有 [UI 使用指南](../../architecture/subsystems/ui/usage-guide.md)和 [UI API 参考](../../architecture/subsystems/ui/README.md)。

## 最小按钮

包含下列头文件后，将函数放在自定义 Scene 的类体中，在 `on_enter()` 中按需调用一次。示例按钮请求退出，点击回调属于输入处理阶段。

```cpp
#include "engine/ui/window/ui_window.h"
#include "engine/ui/widgets/ui_button.h"
```

```cpp
void create_exit_button() {
    auto* window = create_and_add_object<elysia::ui::UiWindow>(
        elysia::core::Rect{40, 40, 300, 160}, 100);
    if (!window) return;
    auto* button = window->create_child<elysia::ui::UiButton>(
        elysia::ui::UiLayoutChildOptions{},
        elysia::core::Rect{0, 0, 180, 44},
        elysia::ui::UiButtonConfig{
            .content = elysia::ui::ui_raw_text("Exit")});
    if (!button) return;
    button->set_on_click([this] { request_quit(); });
}
```

场景持有窗口，窗口或容器持有子节点；返回指针均为借用，不能自行 delete。不要把同一个节点同时交给多个容器。这里的回调捕获场景，因此按钮必须保持在该场景管理的生命周期内。复用场景时避免重复创建窗口；退出时是否保留窗口按业务决定。

标准 Scene 已负责 UI 输入与绘制调度，不要再手动调用一遍。窗口坐标、逻辑 UI 坐标和世界坐标不能直接混用；UI 命中位置以控件最终的 `screen_rect()` 为准。

## 按任务继续阅读

| 任务 | 文档 |
| --- | --- |
| 容器、布局、子节点所有权 | [布局和所有权](../../architecture/subsystems/ui/usage-guide.md#布局和所有权) |
| 本地化文本、主题与样式 | [UI 使用指南](../../architecture/subsystems/ui/usage-guide.md)、[本地化服务](../systems/localization.md) |
| 键盘与手柄焦点导航 | [焦点、键盘和手柄](../../architecture/subsystems/ui/usage-guide.md#焦点键盘和手柄) |
| 滚动、对话框、下拉框与提示 | [滚动、弹窗与 tooltip](../../architecture/subsystems/ui/usage-guide.md#滚动弹窗与-tooltip) |
| 查具体控件接口 | [UI API 参考](../../architecture/subsystems/ui/README.md) |

[返回使用指南](../README.md)
