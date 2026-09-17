# 控制器、命令与角色

## 游戏 API

[ControllerService](../../../engine/gameplay/control/controller_service.h) 是游戏侧单例入口。Manager 是唯一所有者；Service 不另存实例或自行调度。

| 接口 | 用途 |
| --- | --- |
| begin_session / end_session / session_active | 显式本地游戏会话 |
| create<T>(create_info, args...) | 创建并登记，返回 expected<ControllerHandle, ControllerError> |
| get<T>(handle) / describe(handle) | 借用查询与绑定描述 |
| remove(handle) | 移除并立即停止后续交付 |
| bind_target / unbind_target | 显式绑定活动场景对象 |
| replace_input_map | 验证并受控替换本地映射 |

所有 Service 调用与控制器回调均在引擎主线程执行；未来网络工作线程应排队提交到主线程，不直接调用这些接口。

ControllerCreateInfo 显式给出 Scene 或 Session 作用域；Scene 还需 context.token()。创建失败不会隐式开始会话。借用指针不能跨回调、删除或会话边界保留。

## ControlCommand

命令包含控制器句柄、绑定代次、序号、实际执行 tick、持续动作 state、有序 events 和本 tick 的 deltas。通用命令不要求 LocalPlayerId；玩家身份属于 LocalPlayerController。

角色实现 `ControlCommandReceiver::on_control_command(command, fixed_delta)` 和 `on_control_cancelled(reason)`。取消用于清除移动、蓄力等意图，不当作正常松键释放攻击。

引擎没有固定 Move／Jump／Attack 访问器。演示使用游戏层 [CommandView](../../../game/input/command_view.h) 提供便捷读取。内置 EngineCharacter 的构造函数显式接收移动动作 ID，不依赖演示动作集合。

物理角色在命令入口写入意图，在物理参与者 fixed_update 中施力；非物理角色可在命令入口按固定 delta 移动。动画等 Updatable 保持变步长，不重复移动。

## 自定义来源

自定义 Controller 覆盖 `produce_intent(tick, fixed_delta)`，通过 protected `submit(ActionInputResult)` 提交状态、事件和增量。Manager 负责验证、缓存、绑定检查和交付；控制器不能绕过调度直接调用角色接收器。

```cpp
class ConstantController final : public elysia::gameplay::Controller {
public:
    explicit ConstantController(elysia::input::InputActionId axis) : _axis(std::move(axis)) {}
protected:
    void produce_intent(std::uint64_t, double) override {
        elysia::input::ActionInputResult intention;
        intention.frame.set(_axis, elysia::input::InputActionValueType::Axis1D, {0.5f, 0});
        submit(std::move(intention));
    }
private:
    elysia::input::InputActionId _axis;
};
```

本地控制器通过 protected `on_mapped_input(const ActionInputResult&)` 观察唯一活动映射的结果，不另外解析第二套映射；键鼠、手柄和受控换绑保持一致。辅助行为仍延迟到 produce_intent 的固定 tick 执行。

本地控制器可以专门处理游戏工具操作；Collider 示例的查询控制器在固定 tick 执行查询，并通过对象移除通知清理工具引用。角色仍由 Manager 的统一命令入口驱动。

测试控制器验证了无玩家、无输入系统的来源以及接管目标。AI 决策、网络接收、远端命令验证和网络超时策略仍未实现。
