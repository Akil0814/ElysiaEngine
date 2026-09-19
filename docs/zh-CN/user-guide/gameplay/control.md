# 控制器与控制命令

控制器把本地输入或自定义意图转换为固定步消费的 `ControlCommand`。游戏层使用 `ControllerService::instance()`；`ControllerManager` 是调度实现，不是游戏侧操作入口。这里的会话是游戏会话，不代表网络连接。

## 接入顺序

1. 由游戏流程的唯一负责人调用 `begin_session()`，处理返回的 `std::expected`；不要每进入一个子场景都重复开始会话。
2. 进入 `GameplayScene`，取得有效的 `control_context()`。
3. 将目标对象加入该场景。目标必须同时实现 `ControlCommandReceiver`。
4. 创建控制器并绑定目标；检查绑定操作的最终结果。
5. 退出游戏会话时调用 `end_session()`。结束会话会释放控制器，旧句柄不应继续使用。

## 创建本地玩家控制器

以下辅助函数可放在游戏代码中，在活动 `GameplayScene` 的输入或更新逻辑中调用。调用方已开始会话，`player` 已登记在场景玩家注册表，`map` 已按设备配置，`target` 已加入该场景且实现命令接收接口。

```cpp
#include "engine/gameplay/control/controller_service.h"
#include "engine/gameplay/scene/gameplay_scene.h"
#include <utility>

std::expected<elysia::gameplay::ControllerHandle,
              elysia::gameplay::ControllerError>
create_player_control(elysia::gameplay::GameplayScene& scene,
                      elysia::input::LocalPlayerId player,
                      elysia::input::InputActionMap map)
{
    using namespace elysia::gameplay;
    return ControllerService::instance()->create<LocalPlayerController>(
        ControllerCreateInfo{ControllerScope::Scene,
                             scene.control_context().token()},
        player, std::move(map));
}
```

创建结果失败时不取值；成功后保存句柄，并调用 `bind_target(handle, scene.control_context(), target)`。设备与动作映射的具体构造见[动作映射](../../architecture/subsystems/input/action-mapping.md)。句柄不是裸指针，也不延长场景或目标生命。

## 绑定结果必须确认

`bind_target`、`unbind_target`、`replace_input_map` 返回 `ControllerOperation`。调用可能在内部回调边界延迟提交，不能把“函数返回”理解为“绑定完成”。

```cpp
// 接在创建成功之后；scene、target 均满足前述前提。
auto* service = elysia::gameplay::ControllerService::instance();
auto created = create_player_control(scene, player, std::move(map));
if (!created) {
    return; // 由调用方记录 created.error()，中止本次接入。
}
auto handle = *created;
auto operation = service->bind_target(handle, scene.control_context(), target);
if (operation.failed()) {
    auto error = operation.error();
    (void)error; // 记录失败原因。
    (void)service->remove(handle); // 本次新建控制器不再使用。
    return;
}
// 保存 handle；若 operation.pending()，同时保存 operation，后续检查最终状态。
// 只有 operation.succeeded() 后，才宣布目标已经绑定成功。
```

`Pending` 不是失败，也不要在主线程忙等它。后续状态变为 `Failed` 时处理 `error()`，例如上下文失效、目标忙、玩家忙、无效映射或请求被后续操作取代。操作对象只保留结果，不保留控制器、场景或目标。

## 命令消费与取消

目标实现 `on_control_command(const ControlCommand&, double fixed_delta)` 和 `on_control_cancelled(InputCancelReason)`。命令包含持续状态、一次性事件、累计增量以及 tick/sequence。控制器在实际固定步交付命令；事件和增量交付后清空，不能假设每个渲染帧都收到一条命令。

取消回调应清除移动、蓄力等持续意图，避免暂停、设备变化或解绑后沿用旧输入。物理目标可在命令回调中记录意图，再在自己的物理阶段消费；不要同时在逐帧更新中重复推进同一运动。

自定义 AI 等来源继承 `Controller`，在 `produce_intent(tick, fixed_delta)` 中通过受保护的 `submit(ActionInputResult)` 提交结果。无需伪造设备事件；这也不意味着引擎已提供网络同步。

## 作用域与清理

Scene 作用域绑定创建时的上下文 token；上下文重置、场景销毁或会话结束会使相关控制器失效。Session 作用域可跨场景保留控制器，但离开旧场景仍会解绑目标，需要在新场景显式重新绑定。

仅离开缓存场景不等于重置上下文。不要在每次 `on_enter()` 中重复创建未清理的控制器。对象移除、场景离开和暂停有引擎侧取消流程；游戏侧仍需停止使用失效句柄及 `get()` 返回的借用指针。`remove()` 返回错误时应区分已经失效与其他接入问题。

## 参考

- [公开服务](../../../../engine/gameplay/control/controller_service.h)、[操作结果与错误](../../../../engine/gameplay/control/controller_types.h)
- [控制器与目标契约](../../../../engine/gameplay/control/control_command.h)
- [GameplayScene](scene.md)、[返回使用指南](../README.md)
