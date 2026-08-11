# ElysiaEngine 物理与 Gameplay 碰撞文档

> 当前结论：物理模块已经完成公共 API、每 Scene 运行时和生命周期骨架，但仍不是可工作的物理引擎。注册、Collider ID、固定步调度、Tile World 绑定和策略装配已经可用；积分、宽相、窄相、求解、事件生成、Tile 碰撞和查询算法仍未实现。

## 当前实现边界

本轮已经落地：

- `PhysicsBody::type` 与 `BodyType { Static, Kinematic, Dynamic }`；
- 强类型 `PhysicsObjectHandle`，以及由 `PhysicsWorld` 独占管理的单调递增 `ColliderId`；
- mutable/const `ColliderProvider::colliders()`；
- `CollisionTarget`、`CollisionEventPhase`、`CollisionEvent`、`CollisionFrame`；
- `ITileCollisionWorld` 数据契约与每 World 单实例绑定；
- 可维护状态的 `IBroadPhaseIndex` 接口，未来可替换为四叉树；
- `PhysicsSystem::integrate`、`CollisionSystem::evaluate` 的类型化空阶段；
- 每个 `Scene` 独占一个 `PhysicsWorld`，自动注册/注销 Provider，并在非暂停帧调用 `advance(delta)`；
- Gameplay 结构化目标、事件 phase 和 listener 转发契约。

本轮明确未实现：重力和速度积分、任何生产宽相索引、碰撞检测与求解、接触缓存和事件分发、Tile 几何碰撞、Ray/Segment 查询、具体 `GameplayCollisionRuntime`。

文档中“当前实现”描述仓库已有且有测试约束的行为；“后续目标”描述仍需实现的算法或类型。早期章节中的目标 API 已按本轮落地状态更新，算法章节仍是后续实现规范。

## 阅读路线

### 快速确认还缺什么

1. [当前状态与缺失审计](01-current-state-audit.md)
2. [目标架构与一帧数据流](02-target-architecture.md)

### 按顺序实现完整物理系统

1. [当前状态与缺失审计](01-current-state-audit.md)
2. [目标架构与一帧数据流](02-target-architecture.md)
3. [逐类职责](03-class-responsibilities.md)
4. [逐函数实现契约](04-function-responsibilities.md)
5. [碰撞算法与数值约定](05-collision-algorithms.md)
6. [Tile Map 碰撞适配](06-tile-map-collision.md)
7. [Gameplay 碰撞 Runtime](07-gameplay-collision-runtime.md)
8. [实施路线与测试验收](08-implementation-roadmap-and-tests.md)

### 只关注 Tile Map 碰撞

1. [目标架构与一帧数据流](02-target-architecture.md)
2. [碰撞算法与数值约定](05-collision-algorithms.md)
3. [Tile Map 碰撞适配](06-tile-map-collision.md)
4. [实施路线与测试验收](08-implementation-roadmap-and-tests.md#阶段-8tile-collision-world)

## 不可破坏的分层原则

`engine/physics` 只认识 Body、Collider、几何形状、过滤、响应、接触、Tile 碰撞单元和查询，不认识 Actor、玩家、敌人、阵营、攻击或伤害。

```text
engine/physics
    ↑ 只输出几何接触与查询结果
engine/gameplay/collision
    ↑ 将 Collider 绑定为 Body / PushBox / HurtBox / HitBox / Sensor
game 或具体项目
    ↑ 决定 Tile 数据、阵营关系、伤害、硬直、击退和角色规则
```

- category 位由上层定义，物理核心不得硬编码 Player、Enemy、HitBox；
- 项目 Tile Map 通过 `ITileCollisionWorld` 适配，不得进入引擎头文件；
- 伤害和攻击去重发生在 Gameplay 层，不进入检测或响应策略；
- `PhysicsService` 只配置策略工厂；运行状态属于每个 `Scene` 的 `PhysicsWorld`。

## 当前实际调用点

`Scene::register_scene_object_interfaces` 一次发现 Body/Collider Provider 并注册到 `_physics_world`。非暂停更新在普通对象更新后调用：

```cpp
_physics_world.advance(delta);
```

`advance` 已实现固定步累积、每帧最多 8 步和超额完整步丢弃。每个固定步会构造 `PhysicsBodyView` / `ColliderView`，调用当前为空操作的 `PhysicsSystem::integrate` 和 `CollisionSystem::evaluate`；因此本轮不会移动对象、产生接触或通知 listener。`raycast` 和 `segment_cast` 安全返回 `std::nullopt`。

## 默认决策

| 项目 | 首版约定 |
| --- | --- |
| 坐标系 | X 向右、Y 向下 |
| 固定步长 | `1 / 60` 秒 |
| 单帧最大物理步 | 8 |
| 求解迭代 | 4 |
| Body 类型 | Static / Kinematic / Dynamic |
| Tile World | 每个 Scene 一个活动世界 |
| Tile 越界 | 默认 Block，可配置 Empty |
| 接触事件 | Begin / Stay / End |
| Circle CCD | 首版不承诺，保持离散检测 |
| 对象局部 time scale | 首版不参与物理 |

返回：[引擎文档入口](../README.md)
