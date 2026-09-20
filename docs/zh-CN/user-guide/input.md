# 输入与动作映射

应用负责收集 SDL 输入，Scene 负责路由。游戏代码使用引擎输入事件和动作，不另起一套 SDL 轮询。控制角色通常采用 [GameplayScene](gameplay/scene.md) 和[控制器](gameplay/control.md)；菜单快捷键可直接使用 Scene 的扩展点。

## 最小动作映射

以下辅助函数放在游戏输入配置代码中。将成功结果移动给 LocalPlayerController；创建、绑定和消费命令见[控制器指南](gameplay/control.md)。动作名由游戏定义，不是引擎内置协议。

```cpp
#include "engine/input/action/input_action_map.h"
#include <optional>

std::optional<elysia::input::InputActionMap> make_move_map() {
    using namespace elysia::input;
    InputActionMap map;
    InputActionId move{"game.move"};
    if (!map.register_action({move, InputActionValueType::Axis2D}, {
        {move, Button2DInputBinding{
            RawInputControl::KeyA, RawInputControl::KeyD,
            RawInputControl::KeyW, RawInputControl::KeyS}}
    })) return std::nullopt;
    return map;
}
```

`register_action`、`add_binding`、`replace_bindings` 返回 bool，失败时不要继续把配置当作成功。`clear_bindings(action)` 清除该动作的当前绑定，`reset_defaults()` 恢复默认绑定，`reset_state()` 清理映射的输入状态。控制器注册后通过服务的 `replace_input_map` 替换整套映射，不修改其只读查询结果。

## 按钮、轴与增量

| 类型 | 绑定与消费 |
| --- | --- |
| Button | `ButtonInputBinding`；`frame.is_pressed` 查持续按下，事件区分 Started/Canceled |
| Axis1D | 按钮带正负 scale 或 `AxisInputBinding`；`frame.axis1d(action)` |
| Axis2D | `Button2DInputBinding`、`Axis2DInputBinding` 或指定 X/Y 分量；`frame.axis2d(action)` |
| Delta | descriptor 使用 `InputValueSemantics::Delta`，用 `PointerDeltaBinding`；读取结果的 `deltas` |

手柄按键使用 RawInputControl 中的 Gamepad 成员，摇杆使用 RawInputAxis；不要把 SDL 设备索引当 InputSourceId。State 轴使用死区、叠加和限幅。Delta 可以选择 MouseX/MouseY/WheelX/WheelY，保留增量，不能再乘 delta，也不套摇杆死区或 [-1,1] 限幅。同一动作不能混用 State 与 Delta；Button 只能是 State。

例如，为上述 move 加入左摇杆绑定时，在函数返回前执行 `map.add_binding({move, Axis2DInputBinding{RawInputAxis::GamepadLeftX, RawInputAxis::GamepadLeftY}})` 并检查 bool。按钮可使用 `ButtonInputBinding{RawInputControl::GamepadSouth}` 或 `ButtonInputBinding{RawInputControl::KeySpace}`，对应 descriptor 的类型设为 Button。

`InputSnapshot` 包含各输入源的初始和最终状态、有序事件、连接/移除列表及焦点丢失状态。`find(source)` 返回可空借用指针，只在原快照有效且未修改时使用。按钮在同一帧按下又松开时，最终状态不足以表达操作次数；需要次数和顺序的攻击、点击逻辑消费事件。

自定义普通 Scene 可在 `on_routed_input` 中将路由后快照交给持久保存的 `InputActionMap::resolve()`，获得 frame/events/deltas。不要每帧重建映射丢失状态。GameplayScene 已将该阶段交给控制器，不再重复解析和交付。

## 场景快捷键与 UI 消费

在包含 `engine/scene/scene.h` 的派生 Scene 类体中加入以下回调；KeyEscape 仅为本示例自行选择的操作，不是引擎统一快捷键。

```cpp
void on_shortcuts(const elysia::input::RawInputFrame&,
    const std::vector<elysia::input::RawInputEvent>& events) override {
    using namespace elysia::input;
    for (const auto& event : events) {
        if (event.type == RawInputEventType::ControlPressed &&
            event.control == RawInputControl::KeyEscape) {
            consume_input(event);
            request_quit();
            return;
        }
    }
}
```

UI 优先处理输入，消费与捕获影响后续玩法路由。`consume_input` 使用当前事件，不伪造 routing id。`set_shortcut_devices(InputCapture)` 设置快捷键设备范围；`set_ui_gamepad` 选择公共 UI 手柄；`set_ui_interaction_mode` 设置 UI 模式。`set_all_gameplay_input_blocked(true)` 可临时阻断玩法输入，恢复时显式设回 false。

UI 焦点、模态窗口和详细控件用法见 [UI](ui.md)。不要绕过路由从全局原始输入再次执行已被 UI 消费的攻击。输入系统将窗口指针坐标转换为逻辑渲染坐标；世界瞄准仍需[相机转换](camera.md)。

## 本地玩家与设备归属

通过 `Scene::local_players()` 管理玩家；默认主玩家拥有全键盘和鼠标。`create_player()` 新建玩家，`create_partition(name, keys)` 创建键盘分区，`bind_keyboard` 分配分区，`bind_source` 分配鼠标或手柄。各 expected 返回值都要检查。

多人重新分区时，复制 `configuration()`，修改其中 partitions 与 bindings，再调用 `apply_configuration(next)` 一次提交；不要先把仍被主玩家占用的按键分配给第二人。分区按键不得冲突，鼠标和每只手柄只能属于一个玩家。配置失败保留旧配置；控制器映射需要的按键也必须处于玩家权限内。

`on_unassigned_input` 可接收未分配手柄事件，用事件的 source 进行加入或分配，处理成功返回 true。`transfer_source` 转移设备，`replace_gamepad` 替换手柄，`unbind_source` 解除归属；不要把键盘当作一只可独占绑定的手柄。断连或焦点丢失后清除持续意图，不继续沿用上次移动值。

这些是本地玩家与游戏会话能力，不提供网络连接或状态同步。设备配置和动作映射不会自动保存到用户设置。

## 固定步与生命周期

控制器会缓存持续状态、按顺序积累事件与 Delta，再在实际固定 tick 交付。零 tick 时保留一次性输入，下一步消费后清空，补跑的后续步不重复消费增量。每控制器最多 1024 条待消费事件，溢出会取消整份待执行输入并记录诊断。

暂停、解绑、映射替换和设备变化会触发取消或释放/回中门控，目标的取消回调应清空持续动作。映射替换可能返回 Pending，必须按[控制器操作结果](gameplay/control.md)确认最终状态。

## 参考

- [动作定义](../../../engine/input/action/input_action_types.h)、[玩家配置](../../../engine/input/local_player_registry.h)
- [场景](scene.md)、[控制器](gameplay/control.md)、[返回使用指南](README.md)
