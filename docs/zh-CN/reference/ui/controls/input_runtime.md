# 输入与滚动运行时

本文说明 UI 输入转换和滚动计算的内部对象。场景通常直接使用窗口与 `UiScrollContainer`；实现新容器或输入桥接时，再使用这些底层接口。

相关头文件：`ui_input_router.h`、`ui_input_state.h`、`ui_gamepad_scroll_synthesizer.h`、`ui_scroll_state.h`。

| 类型 | 调用方法 | 作用 |
| --- | --- | --- |
| `UiInputRouter` | `route_frame`、`route_event`、`synthesize_events`、`reset_transient_state` | 将原始输入帧与事件转为 UI 输入帧与事件，并生成手柄滚动等合成事件。 |
| `UiInputState` | `set`、`is_pressed`、`is_just_pressed`、`is_just_released` | 保存并查询 UI 动作的按住状态和本帧按下、释放边沿。 |
| `UiGamepadScrollSynthesizer` | `synthesize(raw_frame)`、`reset` | 把连续手柄输入转换为可选滚动事件。 |
| `UiScrollState` | `set_axis`、viewport/content size、偏移、模拟步、比例、`scroll_by` | 计算独立滚动状态并限制偏移范围。 |

`UiScrollContainer` 是场景层首选入口；只有实现新的滚动容器或输入桥接时才直接使用这些运行时对象。

## 场景级输入协调

场景的内部生命周期输入入口委托 `elysia::scene::SceneInputRouter`（`engine/scene/input/`），先将键鼠及指定 UI 手柄的输入交给 `UiInputRouter`，再将未消费的输入交给快捷操作与本地控制器。普通场景无需控制器上下文即可处理 UI。UI 事件消费会阻止对应原始操作进入玩法，持续状态也会被屏蔽到释放或回中。场景 UI 模式使用 `elysia::scene::UiInteractionMode`。

`UiElement::input_capture()` 默认返回无捕获；`UiChildHost` 汇总有效子节点，焦点 `UiTextInput` 报告键盘捕获，`UiWindow` 的模态层和活动弹出层报告捕获。键盘捕获覆盖全部键盘分区；鼠标捕获作用于鼠标所属玩家；手柄捕获只作用于指定 UI 手柄。UI 手柄选择与游戏绑定相互独立。

`cancel_input_interaction()` 清除按钮按住、拖拽等临时交互而保留内容和焦点，用于失焦、设备断开及UI 手柄切换。它与清空控件配置的 reset 不同。

该虚接口允许交互取消通知抛出异常；仅清状态的控件仍可使用 `noexcept` override。`UiChildHost` 和场景输入路由使用稳定生命周期快照完成整批取消，再传播首次异常。HUD [UiActionButton](ui_action_button.md) 会将正在进行的鼠标交互通知为 `Canceled`；其外部状态更新不会产生交互回调。

详细路由与生命周期见 [输入架构](../../../architecture/subsystems/input/architecture.md)。

纯菜单默认 Navigation，可用首个手柄按下取得 UI 操作权，不绑定游戏玩家。玩法场景默认 Pointer，普通 HUD 只响应指针；打开交互菜单显式启用 Navigation。激活文本框始终接管键盘，结束编辑后被抑制按键须释放才能恢复玩法。
