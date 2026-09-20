# 场景系统

`Scene` 负责组织游戏对象与 UI，并协调输入、逐帧更新、物理固定步、相机和绘制。菜单、展示页和简单关卡可以直接继承它；需要控制器与玩法碰撞时使用 [GameplayScene](gameplay/scene.md)。先阅读[核心概念](core_concepts.md)，再按本页组织具体代码。

## 创建与进入场景

以下类型可放入游戏代码的场景头文件。它只有一个静态对象，用于展示复用与重置；如何让对象可见见[游戏对象与绘制](game-objects.md)。

```cpp
#include "engine/scene/scene.h"
#include "engine/scene/scene_manager.h"
#include "engine/scene/routing/scene_payload.h"

struct RoomPayload { int score = 0; };

class RoomScene final : public elysia::scene::Scene {
public:
    void on_enter(const elysia::scene::ScenePayload& payload) override {
        if (const auto* data = elysia::scene::try_scene_payload<RoomPayload>(payload))
            score_ = data->score;
        // 本例允许空 Payload；需要必填参数的场景应显式拒绝类型不符。
        if (!object_)
            object_ = create_and_add_object<elysia::core::GameObject>(
                elysia::core::DepthLayer::Item);
        if (!object_)
            return; // 创建未成功，不执行依赖对象的逻辑。
        object_->set_position({100.0f, 100.0f});
        resume();
    }
    void on_exit() override {
        // 本例保留对象供下次进入；外部订阅应在这里解除。
    }
    void reset() override {
        score_ = 0;
        if (object_) object_->reset();
        resume();
    }
protected:
    void on_scene_object_removing(elysia::core::SceneObject& object) override {
        if (&object == object_) object_ = nullptr;
    }
private:
    elysia::core::GameObject* object_ = nullptr; // 场景持有，这里只借用。
    int score_ = 0;
};

// 由 IGameModule::register_scenes 调用；项目内确保标识符唯一。
inline void register_room(elysia::scene::SceneManager& scenes) {
    scenes.register_game_scene<RoomScene>(1);
}
```

游戏 SceneKey 使用 `1..999`，不能复用引擎保留标识或重复注册。构造参数会被保存用于重建，须满足注册接口的可复制要求；借用依赖用 `std::ref`/`std::cref` 时自行保证生命期。完整应用入口见[游戏初始化](game-initialization.md)。

## 常用操作与扩展点

| 接口 | 使用位置与规则 |
| --- | --- |
| `create_and_add_object<T>(...)` | 创建 GameObject 或 UiElement 派生对象并交给场景；返回借用指针，失败可返回空 |
| `add_object(std::unique_ptr<T>)` | 转移现有对象所有权；调用后不自行释放它 |
| `pause()` / `resume()` | 改变场景暂停状态；暂停不阻止所有 UI、相机和清理流程 |
| `on_update(double)` | 额外逐帧逻辑；每帧调用一次 `Scene::on_update(delta)`，否则对象更新、物理、相机和清理都会被跳过 |
| `on_fixed_update(tick, delta)` | 普通 Scene 的固定步规则；仅实际模拟步发生时执行 |
| `on_shortcuts(frame, events)` | UI 处理后的快捷操作；用 `consume_input(event)` 消费已处理事件 |
| `on_unassigned_input(event)` | 未分配设备输入，例如按键加入；返回是否处理 |
| `on_routed_input(snapshot)` | 经场景路由处理的输入；GameplayScene 已接管此扩展点 |
| `on_pause_changed(bool)` | 普通 Scene 响应暂停变化；GameplayScene 有自己的 final 实现 |
| `on_scene_object_registered` / `on_scene_object_removing` | 同步业务关联；重写注册回调时保留基类行为；移除回调内不要销毁仍在执行的监听者 |
| `physics_world()` / `camera()` | 访问本场景物理世界及当前渲染相机；具体用法见对应专题 |
| `resolve_camera_focus()` / `resolve_camera_focus_rect()` | 提供 Main 相机跟随目标，不保存相机内部状态 |

输入细节见[输入](input.md)，固定步与暂停见[时间](time-and-timers.md)，物理与坐标分别见[物理](physics.md)、[相机](camera.md)。场景的渲染入口是私有非虚函数；通过对象提交命令，不能重写 `on_render()`。

对象正在更新、遍历或查询时，不向同一对象集合插入对象。把生成请求暂存，在派生场景调用基类更新之前或返回之后统一执行；不要在对象自己的 `update()` 中直接扩容场景列表。

## 切换、返回与退出

下面是派生场景输入/更新函数中的片段，目标 2 必须已注册并接受 RoomPayload。调用后结束本次业务分支，避免同一处理周期再发请求。

```cpp
request_scene_switch(2, RoomPayload{42}, elysia::scene::SceneReloadMode::Reset);
return;
```

也可传入 `SceneRoute{.target = ..., .payload = ..., .reload_mode = ...}`，将返回目的地作为值保存。退出应用使用 `request_quit()`。这些受保护接口只在派生场景的正常输入/更新阶段调用；不要在 `on_enter`、`on_exit`、`reset`、绘制或析构中发请求。

请求在当前输入/更新处理结束后执行，不是函数调用时立即换场景。一个处理周期最多一个请求；重复或重入请求、无效或未注册目标属于接入错误，会抛异常，不提供自动回退。启动失败与不可恢复运行错误见[应用错误场景](builtin-scenes/application-failure.md)。

## 复用、重置与释放

| 路由模式 | 目标场景行为 |
| --- | --- |
| `Reuse` | 使用缓存实例，保留业务状态，再次调用 `on_enter`；即使目标是当前实例，也会先退出再进入 |
| `Reset` | 使用实例并调用其 `reset()`，之后进入；清理哪些业务状态由该函数负责 |
| `Recreate` | 销毁目标的旧缓存实例，再创建新实例；旧对象引用全部失效 |

离开场景不等于销毁它。`on_exit()` 负责解除外部监听、停止场景专属声音、清理失效关联；可按玩法保留场景对象。不要每次进入无条件重新创建一套对象或订阅。

`destroy()` 只标记对象；基础更新末尾清理场景对象，UI 子节点由所属容器清理。退出后场景不再更新时，标记对象可能留到下次更新或实际析构；不要依赖它继续可用，也不要等待“下一帧”才解除外部引用。`reset()` 是游戏实现的重置逻辑，不是自动清空整个对象列表。需要全新对象图时可以选择 Recreate。

## 参考

- [Scene 接口](../../../engine/scene/scene.h)、[场景路由](../../../engine/scene/routing/scene_route.h)
- [GameplayScene](gameplay/scene.md)、[返回使用指南](README.md)
