# GameplayScene 使用指南

`GameplayScene` 适用于需要角色控制与玩法碰撞的关卡。普通 `Scene` 已有对象管理、输入路由、UI、相机和物理世界；`GameplayScene` 在此基础上组合控制上下文与玩法碰撞运行时。菜单或纯展示场景不必为使用物理而改成 `GameplayScene`。

普通场景的生命周期、对象管理和切换规则见[场景系统](../scene/scene.md)。本页介绍 GameplayScene 增加的能力与扩展约束。

## 最小场景

以下类型定义放在游戏场景头文件中。它展示合法扩展点，不包含角色、资源和路由注册；注册方法见[游戏初始化](../game-initialization.md)。

```cpp
#include "engine/gameplay/scene/gameplay_scene.h"

class BattleScene final : public elysia::gameplay::GameplayScene
{
public:
    void on_enter(const elysia::scene::ScenePayload&) override {}
    void on_exit() override {}
    void reset() override {}

protected:
    void on_game_fixed_update(std::uint64_t tick, double delta) override
    {
        (void)tick;
        (void)delta;
        // 放置需要在控制命令交付后执行的固定步游戏规则。
    }
};
```

空的进入、退出和重置实现只适合这个没有业务状态的示例。实际场景需要创建或恢复对象，并清理自己的监听器、回调与关联；不要把 `reset()` 当成引擎自动清空全部对象的操作。

## 运行时与扩展点

场景管理器在进入前激活控制上下文，并把本场景的玩法碰撞运行时接到服务上；退出时先解除这些活动关联，再调用 `on_exit()`。所以退出回调中不要依赖全局玩法碰撞服务仍指向自己。可通过受保护的 `collision_runtime()` 清理本场景持有的玩法绑定。

`control_context()` 是本场景的控制上下文，其 token 用于创建 Scene 作用域控制器。上下文由场景拥有，不自行创建、删除或伪造 token。

| 扩展点 | 用途 |
| --- | --- |
| `on_enter`、`on_exit`、`reset` | 游戏自己的进入、退出与重置逻辑 |
| `on_game_fixed_update` | 控制器固定步命令交付后的游戏规则 |
| `on_control_target_removing` | 场景对象移除时处理业务引用；控制器清理已先执行 |
| `on_update` | 额外逐帧逻辑；重写时须调用 `Scene::on_update(delta)` 保留基础调度与清理 |

不要重写 `on_routed_input`、`on_fixed_update`、`on_pause_changed`、`on_scene_object_removing`，它们在 `GameplayScene` 中是 `final`。固定步先推进控制器，再调用 `on_game_fixed_update`，随后物理世界继续该步的参与者更新与模拟。

## 生命周期与暂停

- 暂停会取消控制输入并停止场景物理固定步；不是停止整个应用或所有 UI。
- 离开场景会解绑控制目标，但不等于销毁缓存场景。Scene 作用域控制器在上下文重置或销毁时释放；再次进入时需要按业务恢复绑定，避免无条件重复创建。
- Reset 路由会先重置控制上下文，再执行场景的 `reset()`。Recreate 会销毁旧实例。Session 控制器仍受游戏会话约束，不能把保留句柄等同于保留有效目标。
- 普通对象的延迟移除由基类更新末尾处理；在 `on_exit()` 里调用 `destroy()` 不会立即释放对象。

先建立游戏会话，再创建与绑定控制器；碰撞体实际注册后再关联玩法角色。具体操作分别见[控制器与命令](control.md)、[玩法碰撞](collision.md)。服务绑定失败、无效 token 或错误路由应处理为接入错误，不能绕过状态检查继续使用。

## 参考

- [公开定义](../../../../engine/gameplay/scene/gameplay_scene.h)与[实现](../../../../engine/gameplay/scene/gameplay_scene.cpp)
- [对象生命周期](../core_concepts.md)
- [返回使用指南](../README.md)
- [攻击命中与伤害示例](damage.md)
