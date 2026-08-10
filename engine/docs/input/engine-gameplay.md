# Engine Gameplay 参考

Engine Gameplay 位于 `engine/gameplay`，建立在通用 Action Input 之上。它提供一套适合动作游戏的标准 Action、默认 binding、`GameplayInputFrame` 语义门面和 receiver contract，但不包含 MoonLine 的角色或战斗规则。

## 标准 Actions

| 常量 | 稳定 ID | 类型 | `GameplayInputFrame` 访问器 |
| --- | --- | --- | --- |
| `actions::Move` | `gameplay.move` | Axis2D | `move()` |
| `actions::Jump` | `gameplay.jump` | Button | `jump_pressed()` |
| `actions::Primary` | `gameplay.primary` | Button | `primary_pressed()` |
| `actions::Secondary` | `gameplay.secondary` | Button | `secondary_pressed()` |
| `actions::Guard` | `gameplay.guard` | Button | `guard_held()` |
| `actions::Dash` | `gameplay.dash` | Button | `dash_pressed()` |
| `actions::Pause` | `gameplay.pause` | Button | `pause_pressed()` |

除 `guard_held()` 外，Button 便利访问器都查询 `is_just_pressed`，适合一次性触发；需要读取按住、释放或扩展 Action 时，使用 `frame.actions()` 取得底层 `ActionInputFrame`。

## 默认 Bindings

`make_default_gameplay_input_map()` 每次返回一个独立 `InputActionMap`。

| Action | 键盘 | 鼠标 | 手柄 |
| --- | --- | --- | --- |
| Move | WASD、方向键 | — | DPad、左摇杆 X/Y |
| Jump | Space | — | South / A |
| Primary | J | 左键 | West / X |
| Secondary | K | — | North / Y |
| Guard | L | 右键 | Left Shoulder |
| Dash | Left Shift、Right Shift | — | Right Shoulder |
| Pause | P | — | Start |

Move 使用默认 threshold `0.5` 和 dead zone `0.2`。左摇杆以 `Axis2DInputBinding` 处理径向 dead zone；DPad、WASD 和方向键分别使用四键组合。右摇杆、扳机、Stick Click、Back、Guide、Paddle 和 Touchpad 当前没有标准 gameplay binding。

## GameplayInputFrame

`GameplayInputFrame` 按值拥有一个 `ActionInputFrame`。GameplayScene 分发期间传递的是 `const GameplayInputFrame&`，receiver 不应保存该引用到回调结束之后。

```cpp
void PlayerController::on_gameplay_input_frame(
    const elysia::gameplay::GameplayInputFrame& input)
{
    const elysia::core::Vector2 direction = input.move();
    _player.set_move_direction(direction);

    if (input.jump_pressed())
        _player.try_jump();

    _player.set_guarding(input.guard_held());
}
```

底层 frame 允许查询扩展 Action：

```cpp
if (input.actions().is_just_pressed(MoonlineActions::Transform))
{
    transform_character();
}
```

## 扩展项目 Action

标准 Action ID 不是 enum，项目可以向每个 GameplayScene 的 map 注册额外 Action。由于 `gameplay_input_map()` 是 protected，通常在项目场景的构造或进入逻辑中完成：

```cpp
namespace MoonlineActions
{
inline const elysia::input::InputActionId Transform{"moonline.transform"};
}

BattleScene::BattleScene()
{
    using namespace elysia::input;

    const bool registered = gameplay_input_map().register_action(
        { MoonlineActions::Transform, InputActionValueType::Button },
        {
            { MoonlineActions::Transform,
              ButtonInputBinding{ RawInputControl::KeyT } },
            { MoonlineActions::Transform,
              ButtonInputBinding{ RawInputControl::GamepadEast } }
        }
    );

    if (!registered)
    {
        // 项目应在初始化阶段把重复 ID 或非法 binding 视为配置错误。
    }
}
```

若多个场景需要同一组项目扩展，应由 `game/` 提供一个统一注册函数，避免每个场景复制默认表。引擎的标准工厂不会自动知道 MoonLine 特有 Action。

## Receiver Contracts

- `GameplayInputFrameReceiver::on_gameplay_input_frame` 每个启用 gameplay 输入的场景帧调用一次。
- `GameplayInputEventReceiver::on_gameplay_input_event` 针对本帧每个 Action event 调用；返回 `true` 会消费当前 event，阻止更低优先级 receiver 收到它。
- Receiver 必须同时是 Scene 可拥有的 `SceneObject`，实际使用中通常继承 `GameObject`；仅实现 receiver 接口不会被 Scene 注册。

receiver 的生命周期、排序与暂停规则详见 [GameplayScene 集成指南](gameplay-scene.md)。

## 当前不提供的能力

- binding 的文件持久化与用户配置迁移；
- Input Context 栈或不同角色的共享 profile；
- 右摇杆 Aim/Camera 标准语义；
- 自动阻止 UI 已处理输入进入 gameplay；
- 鼠标位置、滚轮、文本输入和 IME 的 Action 化。
