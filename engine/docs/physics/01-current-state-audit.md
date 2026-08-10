# 01｜当前状态与缺失审计

返回：[物理文档入口](README.md)

## 1. 审计结论

当前物理层已经建立了较清晰的分层方向，但真正的运行闭环尚未形成。可以把现状概括为：

```text
数据契约       已有
策略插槽       已有
Scene 调用点   已有
策略具体实现   缺失
每场景世界     缺失
运动与响应     缺失
事件与查询     缺失
Tile Map       缺失
Gameplay 路由  只有门面，没有 runtime
```

现有测试证明默认值、接口形状、策略安装的事务性，以及 Gameplay Service 的转发行为；它们不证明角色会移动、Collider 会相交或事件会产生。

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
| `Collider` | 部分完成 | ID、形状、过滤、响应、检测模式、单向配置、tag、enabled | ID 分配、注册、世界 Transform、生命周期 |
| `PassThroughDirection` | 完成（契约） | 位组合与查询函数 | 方向判断算法尚未消费它 |
| `CollisionPair` | 建议调整 | 表达两个 Collider ID | 无法表示 Tile World 与 Tile 坐标 |
| `CollisionManifold` | 部分完成 | 法线、穿透深度、最多两个接触点 | 生成规则、有效性约束、数值容差 |
| `CollisionHit` | 部分完成 | manifold 与归一化 TOI | 离散/连续检测实现 |
| `CollisionContact` / `CollisionOverlap` | 部分完成 | Block/Overlap 结果载体 | 统一目标、事件阶段和接触缓存 |
| Query 数据结构 | 部分完成 | Ray、Segment、最近命中字段 | 输入校验、普通 Collider/Tile 查询、最近命中排序 |
| `PhysicsBody` | 建议调整 | 速度、力、限速、重力比例、阻尼、质量、三个状态 bool | bool 状态可冲突；无积分、验证、接地状态和 Transform 写回 |
| `PhysicsBodyProvider` | 部分完成 | 暴露可变/只读 Body | 注册与生命周期协议 |
| `ColliderProvider` | 建议调整 | 暴露只读 Collider span | runtime 无法为 invalid Collider 分配稳定 ID |
| `ColliderView` | 部分完成 | Collider 指针和 previous/current owner origin | 目标身份、Body/owner 句柄、世界形状缓存 |
| Strategy 接口 | 部分完成 | Broad、Discrete/Continuous、Response 插槽 | 没有任何生产实现 |
| `CollisionSystem` | 空壳 | 可设置并查询四个策略 | `dispatch_events` 完全忽略输入；没有帧数据和缓存 |
| `PhysicsSystem` | 空壳 | `step` 模板入口 | 完全忽略 Body 和 delta |
| `PhysicsService` | 完成（配置门面） | 一次配置、独立策略实例、失败不改目标系统 | 尚未自动接入 Scene；不负责世界状态是正确边界 |
| `ICollisionQueryService` | 只有接口 | Ray/Segment 最近命中抽象 | 没有实现对象 |

## 4. Scene 接入盘点

`Scene` 当前会：

1. 在对象加入 Scene 时通过 `dynamic_cast` 查找 Provider；
2. 把对象、`GameObject` 和 Provider 借用指针保存到 entry 数组；
3. 非暂停帧先调用 `PhysicsSystem::step`，再调用 `CollisionSystem::dispatch_events`；
4. 在帧尾清除已经 destroyed 的 entry。

仍然存在以下断点：

- entry 加入时没有注册 Collider，也没有分配 ID；
- 两个 System 没有共享 previous/current Transform、候选对或接触结果；
- `PhysicsSystem` 无法知道哪个 Collider 属于哪个 Body；
- Collider-only 对象与 Body-only 对象没有统一的物理对象身份；
- 对象在本帧 update 中移动后，系统没有可靠的 previous origin；
- destroyed 对象直到帧尾才清理，物理阶段需要显式跳过并在安全点注销；
- `PhysicsService::apply_to` 没有调用点；
- Scene 没有查询服务或 Tile World 的访问入口。

目标设计应把这些状态收进每 Scene 一个 `PhysicsWorld`，Scene 只负责生命周期和调用顺序。

## 5. Gameplay 碰撞盘点

| 类型或模块 | 状态 | 当前具备 | 仍然缺失 |
| --- | --- | --- | --- |
| Gameplay ID 与预设 Team | 完成（契约） | 无效值与 Player/Enemy/Neutral 预设 | 项目可扩展 Team |
| `ColliderBinding` / `HitBoxBinding` | 部分完成 | owner、team、role、instigator、attack IDs | 校验、存储、冲突规则 |
| `ActorCollisionRig` | 部分完成 | Body、PushBox、HurtBox、Sensor ID 集合 | 注册原子性、反向索引、注销 |
| Gameplay Event | 部分完成 | Body、PushBox、Hit 三类载体 | Begin/Stay/End 和 Tile 结构化目标 |
| `GameplayCollisionListener` | 只有接口 | 三个默认空回调 | 无 listener 注册和调用点 |
| `TeamRelationResolver` | 只有接口 | relation 抽象 | 默认或项目实现 |
| `IGameplayCollisionRuntime` | 只有接口 | binding 与 drop-through 操作 | 没有具体 runtime |
| `GameplayCollisionService` | 完成（门面） | attach/detach、无 runtime 日志、转发、drop-through 基础校验 | Scene 生命周期没有 attach；没有事件路由 |

## 6. 完全缺失的首版能力

### 6.1 世界与注册

- 每 Scene 的 `PhysicsWorld`；
- `PhysicsObjectHandle` 与注册表；
- 单调递增且世界生命周期内不复用的 Collider ID；
- Body、Collider、owner Transform 的关联；
- 重复注册、部分注册失败和注销规则；
- 安全的延迟注册/注销队列。

### 6.2 时间与运动

- 固定时间步 accumulator；
- 帧间 previous/current origin；
- Dynamic 的重力、力、质量、阻尼与速度积分；
- Kinematic 的显式速度移动；
- Static 的不可移动约束；
- Transform 写回和本步累计力清空；
- 大帧 delta 的追赶上限与诊断。

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

- `ITileCollisionWorld`；
- Tile 单元语义和越界策略；
- world/tile 坐标转换；
- AABB 候选 Tile 范围；
- Tile 接触的结构化身份；
- 普通 Collider 与 Tile 的 Ray/Segment 最近命中。

### 6.5 Gameplay 与工具

- 具体 `GameplayCollisionRuntime`；
- Physics event 到 Gameplay event 的路由；
- HitBox/HurtBox 命中去重；
- drop-through 忽略对和恢复；
- Collider、候选对、接触法线、Tile 候选范围的调试绘制；
- 固定步丢弃、无效质量、ID 冲突等诊断。

## 7. 必须先解决的接口问题

### `PhysicsBody` 状态冲突

当前 `is_static` 与 `is_kinematic` 可以同时为 true。目标设计使用：

```cpp
enum class BodyType : std::uint8_t
{
    Static,
    Kinematic,
    Dynamic
};
```

### `ColliderProvider` 只读问题

运行时注册需要把有效 ID 写回 Collider，目标接口应增加可变 overload：

```cpp
virtual std::span<Collider> colliders() noexcept = 0;
virtual std::span<const Collider> colliders() const noexcept = 0;
```

### Tile 不能伪装成普通 Collider

为每格分配 ID 会造成大量注册对象；整张地图只用一个 Collider ID 又会丢失 Tile 坐标。目标设计使用 `CollisionTarget` 区分普通 Collider 与 `TileWorld + TileCoordinate`。

### 两个空系统缺少协调者

响应需要同时读取 Collider、Body、Transform、Tile 与接触缓存。继续让 Scene 分别驱动两个互不共享状态的 System 会让依赖扩散。目标设计由 `PhysicsWorld` 协调，两个 System 变为内部算法组件。

## 8. 当前测试能证明什么

现有三组 physics 标签测试主要覆盖：

- 数据结构默认值；
- `PassThroughDirection` 位运算；
- 策略槽所有权；
- `PhysicsService` 配置、独立实例和失败原子性；
- `GameplayCollisionService` attach/detach、转发与错误日志。

它们没有覆盖任何真实移动、相交、响应、事件、Tile 或查询行为。后续每个实施阶段都必须先建立可独立运行的单元测试，不能只依靠 Sandbox 手工观察。

下一篇：[目标架构与一帧数据流](02-target-architecture.md)
