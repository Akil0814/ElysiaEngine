# 09｜首版实现索引与调试

返回：[物理文档入口](README.md)

## 源码索引

| 能力 | 主要实现 |
| --- | --- |
| 固定步、注册、事件、查询 | `engine/physics/physics_world.h/.cpp` |
| Body 积分 | `engine/physics/body/physics_system.h/.cpp` |
| 世界形状与几何 | `engine/physics/collision/world_shape.h/.cpp` |
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

启用 `DebugDraw` 后，Scene 每帧把最近物理步快照提交到已有分类：

| 分类 | 内容 |
| --- | --- |
| `PhysicsCollider` | current 世界形状 |
| `PhysicsCcd` | previous 世界形状 |
| `PhysicsBroadPhase` | swept bounds |
| `PhysicsContact` | 代表接触点 |
| `PhysicsContactNormal` | first → second 法线 |
| `PhysicsVelocity` | 速度向量 |

`PhysicsWorld::last_step_stats()` 返回注册数、proxy、候选、窄检、contact、Tile sample、CCD hit/iteration、solver iteration 和累计 dropped fixed steps。

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
