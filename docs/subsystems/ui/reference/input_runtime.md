# 输入与滚动运行时

覆盖头文件：`ui_input_router.h`、`ui_input_state.h`、`ui_gamepad_scroll_synthesizer.h`、`ui_scroll_state.h`。

| 类型 | 调用方法 | 作用 |
| --- | --- | --- |
| `UiInputRouter` | `route_frame`、`route_event`、`synthesize_events`、`reset_transient_state` | 将 RawInput frame/event 转为 UI frame/event，并生成手柄滚动等合成事件。 |
| `UiInputState` | `set`、`is_pressed`、`is_just_pressed`、`is_just_released` | 保存并查询 UI action 的一帧 pressed/edge 状态。 |
| `UiGamepadScrollSynthesizer` | `synthesize(raw_frame)`、`reset` | 把连续手柄输入转换为可选 scroll event。 |
| `UiScrollState` | `set_axis`、viewport/content size、offset、step、ratio、`scroll_by` | 计算并 clamp 独立滚动状态。 |

`UiScrollContainer` 是场景层首选入口；只有实现新的滚动容器或输入桥接时才直接使用这些运行时对象。
