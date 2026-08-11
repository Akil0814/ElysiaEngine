# Input 测试与调试

## 自动测试入口

| 测试 | 覆盖重点 |
| --- | --- |
| [`input_action_mapping_tests.cpp`](../../../tests/input/input_action_mapping_tests.cpp) | ID 校验、多键绑定、同键多 Action、按钮边沿、事件去重、轴 dead zone、数字/模拟二维合成、运行时改绑 |
| [`engine_gameplay_tests.cpp`](../../../tests/input/engine_gameplay_tests.cpp) | 标准默认 map、GameplayInputFrame 访问器、自定义 Action、标准 Action 换绑 |
| [`gameplay_scene_input_tests.cpp`](../../../tests/input/gameplay_scene_input_tests.cpp) | 普通 Scene 与 GameplayScene 边界、receiver 顺序/消费、inactive、paused、禁用与 destroyed 清理 |

只运行输入测试：

```powershell
ctest --test-dir build -C Debug -L input --output-on-failure
```

修改 Input/Scene 公共头文件后仍应执行完整构建和全部 CTest，因为 UI、Scene、Camera 和 Effects 测试都可能间接包含这些类型。

## 摇杆在小幅移动时没有输出

1. 确认 Raw 层收到正确的 `GamepadLeftX/Y` 或 `GamepadRightX/Y`。
2. 检查 Action 使用 `Axis2DInputBinding` 还是两个 `AxisInputBinding`。
3. 比较输入长度与 descriptor dead zone；标准 Move 是径向 `0.2`。
4. 注意恰好等于 dead zone 仍被视为零，只有严格大于 dead zone 才产生贡献。
5. 如果 frame 有值但没有 `is_pressed`，检查 actuation threshold；Move 默认是 `0.5`。

## 按键有 Raw Input，但 Action 不触发

1. 使用 `map.contains(action)` 确认 Action 已注册。
2. 检查 `map.bindings(action)` 是否为空，以及 binding 内的 Action ID 是否与 descriptor 完全一致。
3. `AnyKey`、`AnyControl`、`None` 和 `Count` 不能作为 Action binding。
4. 检查值类型兼容性：Axis binding 不能绑定 Button；二维 composite 只能绑定 Axis2D。
5. 确认场景继承 `GameplayScene`，且覆盖 `on_input` 时调用了 `GameplayScene::on_input`。

## 同一 Action 出现重复行为

Action Map 每个 Action 每帧最多生成一个 event；如果业务执行两次，优先检查：

- 同一对象是否同时在 frame 的 `is_just_pressed` 和 event 的 `Started` 中执行了相同命令；
- 是否存在两个不同 receiver 都响应同一 event 且前者返回 `false`；
- Action 是否以不同 ID 注册了语义重复的动作；
- 派生 Scene 是否错误地调用了两次 `GameplayScene::on_input`。

## 暂停后仍收到输入

- 检查对象是否调用过 `set_receive_input_when_paused(true)`。
- 暂停只过滤 receiver，不会停止 map 解析；这是为了让允许暂停输入的对象保持连续状态。
- 若希望 gameplay 完全停止，调用 `set_gameplay_input_enabled(false)`，同时确认 UI 仍通过基础 Scene 正常接收。

## 重新启用时立即触发 Started

切换 `set_gameplay_input_enabled` 会 reset Action Map 的 previous values。重新启用时仍处于按住状态的输入会被视为新的 Started。这是当前明确行为，不是 event 重复。

可以在关闭 overlay 后等待相关 Raw Control 释放，再启用 gameplay；不要通过保存旧 `ActionInputFrame` 引用绕过状态机。

## 改绑后边沿全部重置

`replace_bindings`、`clear_bindings` 和 `reset_defaults` 当前都会重置整个 map 的 previous values，而不只目标 Action。换绑操作应放在设置流程边界，避免在正常 gameplay 帧中频繁调用。

`add_binding` 不重置状态；如果在 Action 已 active 时追加 binding，下一次事件以现有 previous value 与新聚合结果比较。

## 调试时应记录什么

建议按层记录，而不是只打印最终动作：

```text
Raw: device, control/axis, raw value
Action: id, value type, previous value, current value, phase
Scene: enabled, paused, receiver object/order, consumed
```

这样可以区分平台翻译、binding、数值过滤和 receiver 分发四类问题。
