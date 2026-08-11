# 01｜当前状态与缺失审计

> **API 骨架落地状态（2026-08）**：本章早期审计所指出的 `BodyType`、mutable `ColliderProvider`、`PhysicsObjectHandle`、结构化 `CollisionTarget`、`CollisionEvent`/`CollisionFrame`、`ITileCollisionWorld`、每 Scene `PhysicsWorld`、固定步累积和 Scene 注册/注销已经实现。旧 `PhysicsSystem::step`、`CollisionSystem::dispatch_events` 与 `IBroadPhaseStrategy` 已删除。生产宽相、积分、检测、求解、事件生成、Tile 算法、查询算法和 Gameplay runtime 仍缺失；以下算法缺失清单继续有效。

返回：[物理文档入口](README.md)

## 1. 审计结论

当前物理层已经建立了较清晰的分层方向，但真正的运行闭环尚未形成。可以把现状概括为：

```text
数据契约       已有
策略/索引插槽  已有
Scene 调用点   已接入 PhysicsWorld
策略具体实现   缺失
每场景世界     生命周期骨架已完成
运动与响应     缺失
事件与查询     契约已有，算法缺失
Tile Map       适配契约已有，算法缺失
Gameplay 路由  只有门面，没有 runtime
```

现有测试还证明 World 注册/注销、ID 生命周期、固定步上限、Tile/listener 身份规则与 Scene 析构顺序；它们仍不证明角色会移动、Collider 会相交或事件会产生。

## 2. 状态定义

| 状态 | 含义 |
| --- | --- |
| 完成 | 当前实现具有实际行为，并有相应测试约束 |
| 部分完成 | 数据或门面可用，但缺少运行时闭环 |
| 空壳 | 类型和入口存在，函数体主动忽略输入 |
| 缺失 | 首版必需，但仓库尚无对应类型或实现 |
| 建议调整 | 现有契约会阻碍正确实现，建议在落地前重构 |

## 3. 物理核心盘点

| 类型或模块 | 状态 | 当前具备 | 仍然缺失或需要调整 |
| --- | --- | --- | --- |
| `AabbShape` / `CircleShape` | 部分完成 | 声明局部矩形、局部圆心和半径 | 参数校验、世界形状转换、相交算法 |
| `ColliderShape` | 完成（契约） | 使用 `std::variant` 保存两种形状 | 首版无需增加新形状 |
| `CollisionFilter` | 部分完成 | category、mask、group 字段 | 双向 mask 和 group 的正式规则与函数 |
| `Collider` | 部分完成 | ID、形状、过滤、响应、检测模式、单向配置、tag、enabled；World 已管理 ID 与注册生命周期 | 世界形状转换与碰撞算法 |
| `PassThroughDirection` | 完成（契约） | 位组合与查询函数 | 方向判断算法尚未消费它 |
| `CollisionTarget` / `CollisionPair` | 完成（契约） | 结构化表达 Collider 或 Tile 坐标；pair 使用两个 target | 算法尚未生成实际 pair |
| `CollisionManifold` | 部分完成 | 法线、穿透深度、最多两个接触点 | 生成规则、有效性约束、数值容差 |
| `CollisionHit` | 部分完成 | manifold 与归一化 TOI | 离散/连续检测实现 |
| `CollisionContact` / `CollisionOverlap` | 部分完成 | 使用结构化 target 的 Block/Overlap 结果载体 | 实际生成与接触缓存 |
| Query 数据结构 | 部分完成 | Ray、Segment、最近命中字段 | 输入校验、普通 Collider/Tile 查询、最近命中排序 |
| `PhysicsBody` | 部分完成 | 速度、力、限速、重力比例、阻尼、质量和单一 `BodyType` | 积分、验证、接地状态和 Transform 写回 |
| `PhysicsBodyProvider` | 部分完成 | 暴露可变/只读 Body | 注册与生命周期协议 |
| `ColliderProvider` | 完成（契约） | 暴露 mutable/const Collider span，注册期要求地址和长度稳定 | 项目 Provider 必须遵守稳定存储约束 |
| `ColliderView` | 部分完成 | object handle、Collider 指针和 previous/current owner origin | 世界形状缓存 |
| Strategy/Index 接口 | 部分完成 | `IBroadPhaseIndex`、Discrete/Continuous、Response 插槽 | 没有任何生产实现 |
| `CollisionSystem` | 空壳 | 拥有 index 与三种 strategy；`evaluate` 提供类型化阶段边界 | evaluate 仅清空 frame，尚不调用任何算法 |
| `PhysicsSystem` | 空壳 | `PhysicsBodyView`、`integrate`、`clear_forces` | 两个函数目前都不修改状态 |
| `PhysicsService` | 完成（配置门面） | 一次配置、独立策略实例、失败不改目标系统 | 尚未自动接入 Scene；不负责世界状态是正确边界 |
| `PhysicsWorld` / `ICollisionQueryService` | 部分完成 | 每 Scene 世界、注册、固定步、Tile/listener 绑定；实现查询接口 | 查询当前固定返回 `nullopt` |

## 4. Scene 接入盘点

`Scene` 当前会：

1. 在对象加入 Scene 时通过 `dynamic_cast` 查找 Provider；
2. 一次调用 `PhysicsWorld::register_object`，保存对象与强类型 handle；
3. 非暂停帧在普通对象更新后调用 `PhysicsWorld::advance(delta)`；
4. 在删除 destroyed 对象前按 handle 注销并清除 Collider ID。

仍然存在以下断点：

- 实际积分、候选、接触与响应阶段仍为空操作；
- previous/current origin 已进入注册记录和 typed views，但尚未被算法消费；
- `PhysicsService::apply_to` 没有调用点；
- Scene 已提供 protected `physics_world()`，但具体 Scene 尚未配置策略或 Tile World。

目标设计应把这些状态收进每 Scene 一个 `PhysicsWorld`，Scene 只负责生命周期和调用顺序。

## 5. Gameplay 碰撞盘点

| 类型或模块 | 状态 | 当前具备 | 仍然缺失 |
| --- | --- | --- | --- |
| Gameplay ID 与预设 Team | 完成（契约） | 无效值与 Player/Enemy/Neutral 预设 | 项目可扩展 Team |
| `ColliderBinding` / `HitBoxBinding` | 部分完成 | owner、team、role、instigator、attack IDs | 校验、存储、冲突规则 |
| `ActorCollisionRig` | 部分完成 | Body、PushBox、HurtBox、Sensor ID 集合 | 注册原子性、反向索引、注销 |
| Gameplay Event | 完成（契约） | Body、PushBox、Hit 三类载体、phase 与结构化目标 | 没有 runtime 生成事件 |
| `GameplayCollisionListener` | 部分完成 | 三个默认空回调；Runtime/Service 有 add/remove 契约 | 没有具体 listener 集合与调用点 |
| `TeamRelationResolver` | 只有接口 | relation 抽象 | 默认或项目实现 |
| `IGameplayCollisionRuntime` | 只有接口 | binding 与 drop-through 操作 | 没有具体 runtime |
| `GameplayCollisionService` | 完成（门面） | attach/detach、无 runtime 日志、转发、drop-through 基础校验 | Scene 生命周期没有 attach；没有事件路由 |

## 6. 完全缺失的首版能力

### 6.1 世界与注册

注册表、稳定 handle/ID、重复注册、失败回滚、注销、reset 与 Scene 析构顺序已经完成。仍缺少事件分发期间使用的 pending 注册/注销队列；当前 API 在 `advance` 重入时安全拒绝修改。

### 6.2 时间与运动

- previous/current origin 的算法消费；
- Dynamic 的重力、力、质量、阻尼与速度积分；
- Kinematic 的显式速度移动；
- Static 的不可移动约束；
- Transform 写回和本步累计力清空；
- 固定步 accumulator 与追赶上限已实现；仍缺少超额时间诊断。

### 6.3 碰撞流水线

- 世界形状转换；
- category/mask/group 过滤；
- Brute Force 粗检；
- AABB/AABB、Circle/Circle、AABB/Circle 离散检测；
- Swept AABB 连续检测；
- Block 位置与速度修正；
- Overlap 非阻挡接触；
- 单向平台响应；
- Begin/Stay/End 接触缓存；
- 稳定排序、去重和事件分发。

### 6.4 Tile Map 与查询

`ITileCollisionWorld`、Tile 单元语义、越界策略、结构化 target 与单 World 绑定已经完成。仍缺少：

- world/tile 坐标转换；
- AABB 候选 Tile 范围；
- 普通 Collider 与 Tile 的 Ray/Segment 最近命中。

### 6.5 Gameplay 与工具

- 具体 `GameplayCollisionRuntime`；
- Physics event 到 Gameplay event 的路由；
- HitBox/HurtBox 命中去重；
- drop-through 忽略对和恢复；
- Collider、候选对、接触法线、Tile 候选范围的调试绘制；
- 固定步丢弃、无效质量、ID 冲突等诊断。

## 7. 已解决的接口问题

### `PhysicsBody` 状态冲突

旧 bool 已删除，当前实现使用：

```cpp
enum class BodyType : std::uint8_t
{
    Static,
    Kinematic,
    Dynamic
};
```

### `ColliderProvider` 只读问题

当前接口已经提供可变 overload，World 可写回并清理 ID：

```cpp
virtual std::span<Collider> colliders() noexcept = 0;
virtual std::span<const Collider> colliders() const noexcept = 0;
```

### Tile 不能伪装成普通 Collider

当前使用 `CollisionTarget` 区分普通 Collider 与 Tile 坐标，不为每格分配 Collider ID。Tile World 身份由当前 `PhysicsWorld` 绑定隐含。

### 两个空系统缺少协调者

Scene 已只驱动 `PhysicsWorld`，两个 System 是 World 内部算法组件。接触缓存仍待加入 World。

## 8. 当前测试能证明什么

现有四组 physics 标签测试主要覆盖：

- 数据结构默认值；
- `PassThroughDirection` 位运算；
- 策略槽所有权；
- `PhysicsService` 配置、独立实例和失败原子性；
- `GameplayCollisionService` attach/detach、转发与错误日志。
- `PhysicsWorld` 注册/注销、ID 不复用、失败回滚、Tile/listener 身份、固定步上限与 Scene 析构顺序。

它们没有覆盖任何真实移动、相交、响应、事件、Tile 或查询行为。后续每个实施阶段都必须先建立可独立运行的单元测试，不能只依靠 Sandbox 手工观察。

下一篇：[目标架构与一帧数据流](02-target-architecture.md)
