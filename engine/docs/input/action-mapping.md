# Action Mapping 详解

对应公开头文件：[`input_action_types.h`](../../input/action/input_action_types.h)、[`action_input_frame.h`](../../input/action/action_input_frame.h)、[`input_action_map.h`](../../input/action/input_action_map.h)。

## InputActionId

`InputActionId` 使用稳定字符串表达逻辑动作。构造函数调用 dotted-key 校验器，每个用 `.` 分隔的组件只能包含 ASCII 字母、数字和下划线；非法 ID 会抛出 `std::invalid_argument`。

```cpp
const elysia::input::InputActionId valid{"gameplay.jump"};
const elysia::input::InputActionId custom{"moonline.transform"};
```

默认构造的 ID 为空且 `valid() == false`，适合作为数据结构的空值；`register_action` 会拒绝空 ID。不要使用数组下标或 enum 序号作为跨模块 Action 身份。

## 值类型

| `InputActionValueType` | 数据含义 | Frame 查询 | 常见用途 |
| --- | --- | --- | --- |
| `Button` | `value.x` 表示按下贡献，`y` 固定为 0 | `is_pressed`、`is_just_pressed`、`is_just_released` | 跳跃、攻击、暂停 |
| `Axis1D` | `x` 为 `[-1,1]` 连续值 | `axis1d`，也可用三个 `is_*` 查询是否越过 actuation threshold | 扳机、油门、缩放 |
| `Axis2D` | `x/y` 为二维连续值 | `axis2d`，也可用三个 `is_*` 查询向量长度是否越过 threshold | 移动、瞄准 |

`InputActionDescriptor` 的默认 `actuation_threshold` 为 `0.5`，默认 `dead_zone` 为 `0.2`。threshold 必须位于 `[0,1]`，dead zone 必须位于 `[0,1)`。

## 四种 Binding

| Binding | 输入源 | 可绑定目标 | 说明 |
| --- | --- | --- | --- |
| `ButtonInputBinding` | 一个真实键盘键、鼠标键或手柄按钮 | Button、Axis1D、Axis2D | 通过 `component` 和 `scale` 决定贡献方向；Button Action 只能使用 X 分量 |
| `AxisInputBinding` | 一个 Raw Axis | Axis1D、Axis2D | Axis1D 只能写 X；Axis2D 可选择 X/Y 分量并缩放 |
| `Axis2DInputBinding` | 一对 Raw Axis | Axis2D | 适合左右摇杆，支持独立 X/Y scale 和径向 dead zone |
| `Button2DInputBinding` | left/right/up/down 四个按钮 | Axis2D | 适合 WASD、方向键和 DPad；相反方向互相抵消 |

Binding 中的 `action` 必须与目标 descriptor ID 完全相同。`None`、`Count` 和 `AnyControl` 一类不可追踪 Control 不能注册为 Action binding。

## 解析与数值规则

`InputActionMap::resolve(raw_frame)` 按注册顺序解析每个 Action：

1. 遍历 Action 的当前 bindings，计算各自贡献。
2. `AxisInputBinding` 对原始单轴值应用绝对 dead zone。
3. `Axis2DInputBinding` 对一对轴应用径向 dead zone；长度大于 1 时先归一化。
4. 所有贡献按 X/Y 分量相加。
5. 最终每个分量分别 clamp 到 `[-1,1]`；Axis1D 和 Button 的 Y 强制为 0。
6. 将最终值与该 Action 上一次 `resolve()` 的值比较，构造 frame state 和 events。

因此数字键盘对角线会保留为 `(1,-1)`，不会被归一化；模拟摇杆自身超过单位圆时会先归一化。键盘、DPad 与摇杆同时输入时会相加，最终逐分量 clamp。

## Frame 查询语义

```cpp
const auto result = map.resolve(raw_frame);
const auto& frame = result.frame;

if (frame.is_just_pressed(jump)) { /* 单次跳跃 */ }
if (frame.is_pressed(guard)) { /* 持续防御 */ }

const float throttle = frame.axis1d(throttle_action);
const elysia::core::Vector2 move = frame.axis2d(move_action);
```

- 查询不存在或类型不匹配的 Action 会返回 `false`、`0` 或零向量，不抛异常。
- Button 以 `x >= actuation_threshold` 判定 pressed。
- Axis1D 以 `abs(x) >= actuation_threshold` 判定 pressed。
- Axis2D 以向量长度达到 threshold 判定 pressed。
- `value(action)` 可读取统一的 `InputActionValue`；`contains(action)` 可先检查 Action 是否存在于本帧。

## Event 合成语义

| Phase | Button | Axis1D / Axis2D |
| --- | --- | --- |
| `Started` | 从低于 threshold 变为达到 threshold | 从 epsilon 范围内的零值变为非零 |
| `Changed` | 保持 active 且值变化超过 epsilon | 保持非零且值变化超过 epsilon |
| `Canceled` | 从达到 threshold 变为低于 threshold | 从非零回到 epsilon 范围内 |

事件变化 epsilon 当前固定为 `0.001`。每个 event 同时保存 `value` 与 `previous_value`。多个 binding 只影响一个 Action 最终值，因此同一 Action 每帧至多产生一个 event。

Action event 只依赖连续 `resolve()` 的结果，不读取 `RawInputEvent[]`。若长时间不调用 `resolve()`，下一次事件会与最后一次已解析状态比较。

## 注册一个完整自定义 Action

```cpp
using namespace elysia::input;

const InputActionId Transform{"moonline.transform"};
InputActionMap map;

const bool registered = map.register_action(
    InputActionDescriptor{
        .id = Transform,
        .value_type = InputActionValueType::Button,
        .actuation_threshold = 0.5f,
        .dead_zone = 0.2f
    },
    {
        InputBinding{ Transform, ButtonInputBinding{ RawInputControl::KeyT } },
        InputBinding{ Transform, ButtonInputBinding{ RawInputControl::GamepadNorth } }
    }
);

if (!registered)
{
    // ID 重复、descriptor 数值非法或 binding 与 descriptor 不兼容。
}

ActionInputResult result = map.resolve(raw_frame);
if (result.frame.is_just_pressed(Transform))
{
    begin_transform();
}

for (const ActionInputEvent& event : result.events)
{
    if (event.action == Transform && event.phase == ActionInputPhase::Canceled)
    {
        end_transform();
    }
}
```

## 运行时修改 Binding

| API | 行为 | 是否清空跨帧状态 |
| --- | --- | --- |
| `add_binding(binding)` | 向已注册 Action 追加一个有效 binding | 否 |
| `replace_bindings(action, bindings)` | 原子替换该 Action 的全部 bindings；任一 binding 非法则不修改 | 是，当前实现会重置所有 Actions |
| `clear_bindings(action)` | 等价于用空数组替换该 Action bindings | 是，当前实现会重置所有 Actions |
| `reset_defaults()` | 所有 Actions 恢复注册时的 default bindings | 是 |
| `reset_state()` | bindings 不变，只清空所有 previous values | 是 |

```cpp
map.replace_bindings(
    elysia::gameplay::actions::Jump,
    {
        { elysia::gameplay::actions::Jump,
          ButtonInputBinding{ RawInputControl::KeyK } }
    }
);
```

`bindings(action)` 返回指向内部 vector 的 `std::span`。后续追加、替换、清除或恢复 binding 后，不应继续保存旧 span。

当前 API 只修改内存中的 map。退出进程后不会保存；这里也没有配置文件 schema 或 Input Context 切换机制。
