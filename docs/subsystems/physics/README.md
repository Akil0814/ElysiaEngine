# ElysiaEngine 2D 物理系统

> 状态（2026-08）：无旋转 2D 首版运行闭环已经落地。默认 `PhysicsWorld` 使用 SAP 扫描线宽相，可直接完成 Body 积分、普通 Collider、规则 Tile、AABB CCD、Block/Overlap、单向平台、Begin/Stay/End、Ray/Segment、Gameplay 路由以及调试快照。本文档中的“目标设计”章节保留设计理由；当前代码事实以本页、[当前实现审计](01-current-state-audit.md)和头文件为准。

## 已实现能力

- `Static`、`Kinematic`、`Dynamic` Body，以及重力、力、质量、阻尼、分轴限速和半隐式欧拉积分；
- 每个 `Scene` 独占一个 `PhysicsWorld`，Provider 自动注册，Collider ID 单调分配且不复用；
- `BruteForceBroadPhaseIndex` 正确性基线和默认 `SweepAndPruneBroadPhaseIndex`；
- AABB/AABB、Circle/Circle、AABB/Circle 离散检测；
- AABB/AABB、AABB/Tile 的 Swept AABB CCD；Circle Continuous 明确回退离散；
- 逆质量加权 Block 求解、Overlap、过滤、四方向单向平台和 drop-through；
- `ContactCache` 与稳定排序的 Begin/Stay/End 核心事件；
- 规则 Tile World、负坐标、非零原点、非正方形 Tile 和可配置越界策略；
- AABB、Circle、Tile DDA 的 RayCast/SegmentCast 最近命中；
- `GameplayCollisionRuntime` 的 Body、PushBox、HitBox/HurtBox 路由、Team 判定和攻击实例去重；
- Scene 自动 attach/detach Gameplay runtime；
- 物理步统计、调试快照以及到现有 `DebugDraw` 的适配。

## 关键入口

```cpp
elysia::physics::PhysicsWorldConfig config;
config.gravity = {0.0f, 980.0f}; // Y 向下

elysia::physics::PhysicsWorld world(config); // 默认 SAP 与完整策略集
auto handle = world.register_object(object, body_provider, collider_provider);
world.advance(frame_delta_seconds);

auto hit = world.raycast({origin, direction, max_distance, filter});
auto state = world.contact_state(handle);
```

自定义宽相或测试 oracle 必须整体注入不可部分构造的 `CollisionStrategySet`：

```cpp
elysia::physics::PhysicsWorld world(
    config,
    elysia::physics::make_brute_force_collision_strategies());
```

旧 `PhysicsService`、策略工厂、CollisionSystem 单槽 setter、`PhysicsBodyView`、`ColliderView` 和独立 `clear_forces` 已删除，不提供兼容包装。

## 阅读路线

1. [当前状态与剩余限制](01-current-state-audit.md)
2. [目标架构与一帧数据流](02-target-architecture.md)
3. [逐类职责](03-class-responsibilities.md)
4. [逐函数实现契约](04-function-responsibilities.md)
5. [碰撞算法与数值约定](05-collision-algorithms.md)
6. [Tile Map 碰撞适配](06-tile-map-collision.md)
7. [Gameplay 碰撞 Runtime](07-gameplay-collision-runtime.md)
8. [实施路线与测试验收](08-implementation-roadmap-and-tests.md)
9. [首版实现索引与调试](09-v1-implementation-reference.md)

## 分层原则

```text
game / 项目 TileMap
        ↓ 适配与业务规则
engine/gameplay/collision
        ↓ Actor、Team、Attack 到 Collider 的绑定
engine/physics
        ↓ 只处理 Body、形状、目标、接触和查询
engine/core
```

`engine/physics` 不认识 Actor、阵营、HitBox、攻击或伤害；项目 Tile 类型只能通过 `ITileCollisionWorld` 适配，不能进入物理核心头文件。

## 首版明确不支持

旋转、角速度、OBB、多边形、斜坡、摩擦、弹性、关节、睡眠和 Circle CCD 不在首版范围。Kinematic 是无限质量移动平台语义，不会被 solver 推动。一个 Scene 当前只绑定一个活动 Tile Collision World。

返回：[引擎文档入口](../README.md)
