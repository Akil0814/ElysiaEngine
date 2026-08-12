# 09｜首版实现索引与调试

返回：[物理文档入口](README.md)

## 源码索引

| 能力 | 主要实现 |
| --- | --- |
| 固定步、注册、事件、查询 | `engine/physics/physics_world.h/.cpp` |
| Body 积分 | `engine/physics/body/physics_system.h/.cpp` |
| 世界形状与几何 | `engine/physics/collision/world_shape.h/.cpp` |
| 材质归一化与合并 | `engine/physics/collision/physics_material.h` |
| Brute、SAP、离散/CCD/响应策略 | `engine/physics/collision/default_collision_strategies.h/.cpp` |
| 候选、Tile、求解 | `engine/physics/collision/collision_system.h/.cpp` |
| Begin/Stay/End | `engine/physics/collision/contact_cache.h/.cpp` |
| 统计与调试数据 | `engine/physics/physics_world_stats.h` |
| DebugDraw 适配 | `engine/physics/physics_debug_draw.h/.cpp` |
| Gameplay 路由 | `engine/gameplay/collision/gameplay_collision_runtime.h/.cpp` |
| Scene 自动接入 | `engine/scene/scene.cpp`、`engine/scene/scene_manager.cpp`、`engine/gameplay/scene/gameplay_scene.cpp` |

## 策略注入

`CollisionStrategySet` 必须一次提供 broad phase、discrete、continuous、response 四个非空实例。构造失败不会产生部分可用的 CollisionSystem。默认函数返回 SAP；`make_brute_force_collision_strategies()` 用于 oracle、回归测试和小规模诊断。

新增四叉树时实现 `IBroadPhaseIndex` 的 `synchronize`、`collect_pairs`、`query_aabb` 和 `clear`，再整体放入 strategy set。输出必须清空传入 vector、规范化 pair、排序并去重。

## 调试

启用 `DebugDraw` 后，Scene 在物理步前把当前分类映射为 `PhysicsDebugCapture`，并只采集、提交已请求的数据：

| 分类 | 内容 |
| --- | --- |
| `PhysicsCollider` | current 世界形状 |
| `PhysicsCcd` | previous 世界形状 |
| `PhysicsBroadPhase` | swept bounds |
| `PhysicsContact` | 代表接触点 |
| `PhysicsContactNormal` | first → second 法线 |
| `PhysicsVelocity` | 速度向量 |

`PhysicsCollider`/`PhysicsCcd` 映射到 `Shapes`，`PhysicsBroadPhase` 映射到 `BroadPhase`，两个 contact 分类映射到 `Contacts`，`PhysicsVelocity` 映射到 `Velocities`。全局关闭或只启用非物理分类时，`PhysicsWorld` 使用 `PhysicsDebugCapture::None`，不会复制 shape、pair、Tile candidate、contact 或 velocity。修改 capture 会立即清空旧快照，避免重新启用时显示过期数据。

`DebugDraw::draw_*` 只接受当前已启用的单一分类；全局或分类关闭时不会创建命令。关闭全局开关会清空全部命令，移除分类会清空该分类命令，重新启用不会恢复旧请求。

当构建启用 `ELYSIA_ENABLE_IMGUI` 时，三个 Physics Demo 还会通过共同基类注册同一个 Physics Inspector。它只读取 `last_step_stats()`、`debug_snapshot()` 和 `PhysicsWorldConfig`，调试复选框直接修改既有 `DebugDraw` 分类；下一次 Scene update 仍由上述映射决定 `PhysicsDebugCapture`，Inspector 不参与物理模拟。生命周期和发布成本见 [Development Overlay](../development-overlay.md)。

`PhysicsWorld::last_step_stats()` 返回注册数、proxy、候选、窄检、contact、Tile sample、被拒绝的 Tile 候选范围、CCD hit/iteration、solver iteration 和累计 dropped fixed steps。统计与物理事件不受 Debug Capture 开关影响。

## Tile 适配最小骨架

```cpp
class ProjectTileCollisionAdapter final
    : public elysia::physics::ITileCollisionWorld
{
public:
    elysia::core::Vector2 world_origin() const noexcept override;
    elysia::core::Vector2 tile_size() const noexcept override;
    int columns() const noexcept override;
    int rows() const noexcept override;
    elysia::physics::TileOutOfBoundsPolicy out_of_bounds_policy() const noexcept override;
    elysia::physics::TileCollisionCell cell_at(
        elysia::physics::TileCoordinate coordinate) const noexcept override;
};
```

适配器由项目/Scene 持有，生命周期必须覆盖 `set_tile_world` 到 `clear_tile_world`。引擎不 include 项目 TileMap 类型，也不会为每个 Tile 分配虚拟 ColliderId。

## 事件使用提醒

核心 listener 接收规范化 `CollisionPair`，法线始终从 pair.first 指向 pair.second。若调用方关注 second，必须反转法线。Gameplay Runtime 已完成该角色路由，不应在物理核心中加入 Actor 或伤害判断。

## 材质与求解

`Collider` 和 `TileCollisionCell` 都携带 `PhysicsMaterial`。求解前会把非有限/负摩擦归零、保证 static friction 不小于 dynamic friction，并把 restitution 钳制到 `[0,1]`；双方摩擦取几何平均、弹性取较大值。

Block contact 使用默认 8 次稳定顺序冲量：位置修正与速度冲量分离，先法向后切向。低于 `restitution_velocity_threshold` 的闭合速度不回弹。Kinematic 保持零逆质量，但 authored velocity 参与相对速度和摩擦，因此可携带 Dynamic。`CollisionContact` 的 `normal_impulse` / `tangent_impulse` 是本固定步累计值，不跨帧 warm start。

## 查询入口

- `raycast` / `segment_cast`：最近命中；
- `raycast_all` / `segment_cast_all`：按 distance、target 排序的全部命中；
- `overlap_aabb` / `overlap_circle`：按 target 排序的当前重叠；
- `sweep_aabb`：最近 AABB/Tile TOI，Circle 目标不参与。

所有查询使用当前已提交状态，Ignore 不命中，Overlap/Block 返回；不会推进模拟或修改 contact、event、stats。Gameplay Sensor 仅把物理 Overlap 的 Sensor↔Body 路由为 `SensorOverlapEvent`，Begin/Stay/End 全部转发。

## 稳定性与失败边界

- `PhysicsWorld::reset()` 步外立即执行，advance/事件回调中延迟；当前 listener 批次完成后 reset 优先于其他 pending 操作，并停止本次 advance 的剩余固定步；
- listener 回调不得抛异常。异常解除 Physics/Gameplay guard 后传播至 Application update boundary，由 `UnhandledException`/FaultExit 终止；不做回滚，异常后不保证 runtime 可继续使用；
- Gameplay Runtime 在回调前复制当前核心事件的全部语义路由，支持回调中安全 unbind/clear/end-attack；`unbind_actor` 一次清理 rig、owner bindings 和相关 hurt history；
- Tile 模拟使用半开 checked range，overlap/sweep 使用闭合 checked range；不可表示坐标、候选溢出或超过配置上限时安全拒绝 Tile 部分；
- Query 读取 registration 的最后提交 origin。AABB sweep 只考虑 AABB/Tile，即使零位移也不会通过通用 overlap 意外命中 Circle。
