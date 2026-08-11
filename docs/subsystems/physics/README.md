# ElysiaEngine 物理与 Gameplay 碰撞文档

> 当前结论：物理模块目前是**可编译的契约骨架**，不是可工作的物理引擎。`Scene` 已经能够发现 Body/Collider Provider，并会调用系统入口；但运动积分、碰撞检测、阻挡修正、事件和查询尚未执行。

## 文档边界

本文档集同时记录两类内容：

- **当前实现**：仓库里已经存在并由测试约束的类型或行为；
- **目标设计**：为了完成第一版 2D 物理而建议新增或调整的接口。目标设计不会被描述成已经可用。

第一版目标是无旋转的 2D AABB/Circle 物理、规则 Tile Map、单向平台、Block/Overlap、Ray/Segment 查询，以及 Gameplay Body/PushBox/HitBox 事件路由。旋转、斜坡、多边形、摩擦、弹性、关节和睡眠不在首版范围内。

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

因此：

- category 位由上层定义，物理核心不得硬编码 Player、Enemy、HitBox；
- Tile Map 的数据结构属于项目，物理核心只声明 `ITileCollisionWorld`；
- `CollisionResponse::Overlap` 是物理层唯一的触发语义；
- 伤害和攻击去重发生在 Gameplay 层，不进入窄检或响应策略；
- `PhysicsService` 是策略配置入口，不是全局物理世界；每个 `Scene` 应拥有自己的 `PhysicsWorld`。

## 当前实际调用点

`Scene::register_scene_object_interfaces` 会登记实现 `PhysicsBodyProvider` 或 `ColliderProvider` 的 `GameObject`。非暂停帧中，`Scene::on_update` 目前依次调用：

```cpp
_physics_system.step(_physics_body_entries, delta);
_collision_system.dispatch_events(_collider_entries, delta);
```

这两个入口当前都不会修改对象或产生事件。实施目标架构后，`Scene` 应只把注册与 `advance(delta)` 交给自己的 `PhysicsWorld`，由世界内部保证固定步进、检测、响应和事件顺序。

## 实现时的默认决策

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
