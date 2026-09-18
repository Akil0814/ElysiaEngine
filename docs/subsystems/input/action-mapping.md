# 动作映射

`InputActionMap` 只解析 `InputSnapshot`。基于各源帧初状态和有序原始事件重放，再与帧末状态协调。同帧按下再松开、多次点击都保留动作事件顺序；帧状态只表达最后持续值。

游戏定义 InputActionId、按键和手柄映射。仓库演示映射位于 [game/input](../../../game/input/gameplay_input_map.cpp)，不是引擎协议。UI 动作保持独立。

## 数值语义

InputActionDescriptor 的 `semantics` 为 `State` 或 `Delta`，默认 State。按钮只能是 State。持续轴使用 ButtonInputBinding、AxisInputBinding、Axis2DInputBinding 或 Button2DInputBinding，继续使用死区、叠加及限幅。

Delta 使用 PointerDeltaBinding，选择 MouseX、MouseY、WheelX、WheelY、目标分量和比例。使用逻辑坐标转换后的事件增量，不读取帧汇总再次累计，不套用摇杆死区或 [-1,1] 限幅，不乘帧时间，也不产生 Started／Canceled 事件。

```cpp
using namespace elysia::input;
InputActionId look{"mygame.look_delta"};
InputActionMap map;
const bool registered = map.register_action(
    {look, InputActionValueType::Axis2D, 0.5f, 0.2f, InputValueSemantics::Delta},
    {{look, PointerDeltaBinding{PointerDeltaAxis::MouseX}},
     {look, PointerDeltaBinding{PointerDeltaAxis::MouseY, InputActionComponent::Y}}});
```

同动作不能混合 State 与 Delta。鼠标瞄准增量和摇杆转向速度应使用不同动作，由游戏决定如何合成。注册、增加和替换绑定返回验证结果；非法类型或非有限比例被拒绝。

## 控制器中的映射

每个 LocalPlayerController 只激活一套映射。游戏可以在登记前构造映射；登记后只允许通过 `ControllerService::replace_input_map(handle, map)` 替换，`input_map()` 只有 const 查询。

替换验证成功后取消旧输入、清空状态／事件／增量并建立释放／回中门控。校验失败保留原映射。替换返回 ControllerOperation，回调内 Pending 需要在安全边界后检查最终结果。不提供可变映射引用，也不自动叠加步行、载具和观战映射。

## 命令缓存

持续值保存最新结果，事件保留次数和顺序。零 tick 时事件和增量跨帧累积，下一实际 tick 交付后清空，后续补跑 tick 的增量为零。UI 消费、取消、解绑和映射切换清理相应待执行输入。

每控制器最多 1,024 条待消费事件；溢出取消整份待执行输入并记录诊断，不截取部分攻击。非有限状态、事件或累计增量不能进入交付缓存。

当前不实现复杂组合键、映射栈优先级、配置持久化或相对鼠标模式。

## 游戏控制方案与键盘分区

游戏提供 `make_gameplay_input_map(InputScheme)`，KeyboardScheme 选择 Wasd、Arrows 或 None，mouse／gamepad 决定是否组合对应绑定。控制器仍只有一套活动映射。WASD 与方向键不默认叠加；示例方向键方案只定义移动，不强行安排攻击或拾取。

`keyboard_keys(scheme)` 提供方案所需键集合。创建分区后先原子配置玩家归属，再创建／绑定相应映射。已绑定控制器的映射替换和分区修改均检查按键权限；无对应设备时允许保留其映射。拾取、开门和地图交互属于玩法动作，UI 只展示提示；不由提示控件直接监听并执行世界行为。

## 精度与参数校验

原始滚轮和 UI 滚轮值使用 float，WheelX／WheelY 增量完整保留小数；滚动容器按小数比例移动，合成手柄滚动仍采用原有整步节奏。Button／Axis1D 绑定仅接受 X 分量，Axis2D 才接受 Y；非法枚举、轴范围和非有限比例被拒绝，失败替换不修改原映射。
