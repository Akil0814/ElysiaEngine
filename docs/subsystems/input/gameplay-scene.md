# GameplayScene 集成指南

`elysia::gameplay::GameplayScene` 是可选的 Scene 基类。只有真正需要 Move、Jump、Attack 等语义输入的实际游玩场景才应继承它；Loading、Menu、Setting 和纯 UI 场景继续直接继承 `elysia::scene::Scene`。

## 最小场景

```cpp
class BattleScene final : public elysia::gameplay::GameplayScene
{
public:
    void on_enter(const elysia::scene::ScenePayload&) override;
    void on_exit() override;
    void reset() override;
};
```

若派生场景覆盖 `on_input`，必须显式调用 gameplay 基类，否则基础 Raw/UI 和 gameplay 两条分发链都会被跳过：

```cpp
void BattleScene::on_input(
    const elysia::input::RawInputFrame& frame,
    const std::vector<elysia::input::RawInputEvent>& events)
{
    elysia::gameplay::GameplayScene::on_input(frame, events);
    // 场景自己的后处理。
}
```

## Receiver 示例

```cpp
class PlayerInputController final
    : public elysia::core::GameObject
    , public elysia::gameplay::GameplayInputFrameReceiver
    , public elysia::gameplay::GameplayInputEventReceiver
{
public:
    PlayerInputController()
        : GameObject(elysia::core::DepthLayer::Character) {}

    void on_gameplay_input_frame(
        const elysia::gameplay::GameplayInputFrame& input) override
    {
        _move = input.move();
        _guarding = input.guard_held();
    }

    bool on_gameplay_input_event(
        const elysia::input::ActionInputEvent& event) override
    {
        using namespace elysia::gameplay;
        using namespace elysia::input;

        if (event.action == actions::Jump
            && event.phase == ActionInputPhase::Started)
        {
            request_jump();
            return true;
        }
        return false;
    }

private:
    void request_jump();
    elysia::core::Vector2 _move{};
    bool _guarding = false;
};

void BattleScene::on_enter(const elysia::scene::ScenePayload&)
{
    create_and_add_object<PlayerInputController>();
}
```

对象必须通过 Scene 的 `add_object` 或 `create_and_add_object` 加入，GameplayScene 才会在 `on_scene_object_registered` 中发现 receiver 接口。

## 分发顺序与事件消费

receiver 沿用 Scene 输入排序：

1. UI receiver 优先于 GameObject。
2. GameObject 中 depth layer 更高者优先。
3. 同一 depth layer 中 `order_in_layer` 更高者优先。

Frame 会发送给所有符合条件的 receiver，不支持消费。Event 按 Action event 外层、receiver 内层遍历；某 receiver 返回 `true` 只消费当前 event，下一个 Action event 仍从最高优先级 receiver 开始。

GameplayScene 在每次输入分发前移除 destroyed receiver。inactive 对象不会接收；invisible 不影响输入资格。

## 暂停行为

GameplayScene 与基础 Scene 共用 `_paused`：

- 场景未暂停时，所有 active receiver 都可以接收。
- 场景暂停时，只有 `receive_input_when_paused() == true` 的对象接收。
- `GameObject` 可调用 `set_receive_input_when_paused(true)`，适用于暂停菜单控制器等明确例外。

```cpp
pause_menu_controller->set_receive_input_when_paused(true);
pause();
```

暂停不会停止 `InputActionMap::resolve`。因此允许暂停输入的 receiver 仍会得到连续 frame/event 状态；其他 receiver 只是被分发过滤。

## 临时禁用 Gameplay Input

`set_gameplay_input_enabled(false)` 完全跳过 Action resolve 和 gameplay receiver 分发，但 `Scene::on_input` 已经先执行，所以 Raw/UI 输入仍正常工作。

```cpp
void BattleScene::open_pause_overlay()
{
    set_gameplay_input_enabled(false);
    pause();
}

void BattleScene::close_pause_overlay()
{
    resume();
    set_gameplay_input_enabled(true);
}
```

开关值发生变化时，GameplayScene 会调用 `InputActionMap::reset_state()`。如果玩家在重新启用时仍按住某个键，下一次 `resolve` 会把它视为从零进入 active 状态，产生 `is_just_pressed == true` 和 `Started`。调用方若不希望关闭菜单的按键立即触发角色动作，需要在 UI 流程中等待释放后再启用 gameplay 输入。

重复设置相同 enabled 值不会再次 reset state。

## 修改场景的 Action Map

派生场景可通过 protected `gameplay_input_map()` 访问自身 map：

```cpp
gameplay_input_map().replace_bindings(
    elysia::gameplay::actions::Dash,
    {
        { elysia::gameplay::actions::Dash,
          elysia::input::ButtonInputBinding{
              elysia::input::RawInputControl::GamepadEast } }
    }
);
```

每个 GameplayScene 构造时都创建独立默认 map；修改一个场景不会自动影响其他场景。当前没有共享 binding profile 或持久化服务。

## 常见错误

- 让 Menu/Setting 继承 GameplayScene：会无意义地产生 gameplay Actions，应保持基础 Scene。
- 派生 `on_input` 只调用 `Scene::on_input`：UI 正常但 gameplay receiver 永远不运行。
- 直接 new receiver 而不加入 Scene：不会触发 receiver 注册。
- 把 Frame 引用保存到下一帧：GameplayScene 分发的是临时 `GameplayInputFrame`，回调结束后引用失效。
- 认为 UI 消费会自动阻止 gameplay：两套消费链相互独立，应显式暂停或禁用 gameplay。
