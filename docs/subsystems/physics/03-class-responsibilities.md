# 03｜逐类职责

> **命名更新**：`BodyType`、`PhysicsWorldConfig`、`PhysicsWorld`、`PhysicsObjectHandle`、`CollisionTarget`、`CollisionEvent`、`CollisionFrame`、`ITileCollisionWorld` 和 `Gameplay` listener 契约现已存在。宽相扩展点最终采用可维护状态的 `IBroadPhaseIndex`，不再采用一次性 `IBroadPhaseStrategy`；生产实现（包括 Brute Force 与四叉树）仍待新增。

返回：[物理文档入口](README.md)　上一篇：[目标架构](02-target-architecture.md)　下一篇：[逐函数实现契约](04-function-responsibilities.md)

## 1. 阅读说明

本篇先回答“每个类型为什么存在、拥有什么、不拥有什么”。函数的逐项输入、输出和失败规则见下一篇。

- **当前类型**：仓库已经声明；描述必须与当前代码一致。
- **目标类型**：建议在实现阶段新增或重构；名称是首版统一采用的建议名称。
- 数据结构应尽量保持简单；需要维护跨帧状态或所有权时才使用 class。

## 2. Body 与运动

### `PhysicsBody`（当前契约已整理）

**目的**：保存一个对象参与物理积分所需的运动状态，不保存世界位置。位置事实仍由 owner `GameObject` 持有。

**当前字段**：velocity、accumulated_force、max_speed、gravity_scale、linear_damping、mass、enabled、type。

**当前已落地与后续约束**：

- `BodyType type` 已替代旧的两个状态 bool；
- `mass` 只对 Dynamic 生效且必须有限并大于零；
- `max_speed.x/y <= 0` 表示该轴不限制，正值表示绝对速度上限；
- `linear_damping` 必须有限且非负；
- `accumulated_force` 每个固定步结束后清零；
- 首版不在 Body 内缓存 grounded/wall 状态，这些是由接触结果派生的 runtime 状态。

**不负责**：Transform、Collider 所有权、碰撞检测、事件或 Gameplay 状态。

**完成标准**：三种 BodyType 不会产生矛盾；相同输入在固定 dt 下积分结果稳定；无效参数不会产生 NaN。

### `BodyType`（当前存在）

| 值 | 是否积分力/重力 | 是否按 velocity 移动 | Block 求解逆质量 |
| --- | --- | --- | --- |
| `Static` | 否 | 否 | 0 |
| `Kinematic` | 否 | 是 | 0 |
| `Dynamic` | 是 | 是 | `1 / mass` |

Kinematic 可以推动 Dynamic，但不会被求解器反推。两个 Static/Kinematic Block 命中只产生接触，不做位置修正，因为双方总逆质量为零。

### `PhysicsSystem`（当前存在但为空壳）

**目的**：只负责 Body 的数值积分、速度约束、owner Transform 写回和力清理。

**拥有**：不拥有长期世界状态；可以持有无状态的积分配置或辅助函数。

**不负责**：候选收集、形状相交、接触缓存、Tile 查询、Gameplay 事件。

**协作**：由 `PhysicsWorld::fixed_step` 调用。积分完成后，`CollisionSystem` 使用更新后的 current origin 检测；求解器修正 Transform/velocity 后，PhysicsSystem 清力。

**完成标准**：Static、Kinematic、Dynamic 行为分别满足表格；禁用 Body 不被修改；无效 dt 安全返回。

## 3. Collider 与形状

### `AabbShape`（当前存在）

保存相对 owner origin 的 `local_rect`。矩形必须保持非负尺寸；空矩形不产生 Block/Overlap，也不参与查询命中。

不保存 world rect，不缓存 Transform，不拥有 Collider。世界 AABB 每固定步由 `ColliderView` 或几何辅助函数计算。

### `CircleShape`（当前存在）

保存相对 owner origin 的 `local_center` 和 radius。radius 必须有限且非负；零半径首版视为空形状，不作为零厚度 Block Collider。

### `ColliderShape`（当前存在）

`std::variant<AabbShape, CircleShape>` 是持久 Collider 允许的形状集合。首版算法必须穷尽 visit 两种类型，不能用默认分支静默忽略未知形状，以便未来扩展时由编译器暴露遗漏。

### `CollisionFilter`（当前存在）

**目的**：物理层的第一阶段资格过滤。

- category：本 Collider 属于哪些类别；
- mask：愿意与哪些类别形成候选；
- group：同组强制覆盖规则。

目标规则是双向 mask；同组正值强制允许、同组负值强制忽略、其他情况使用 mask。它不认识 TeamRelation。

### `CollisionResponse`（当前存在）

- `Ignore`：不产生接触、不求解；
- `Overlap`：产生接触事件，不修正位置或速度；
- `Block`：产生接触并进入求解。

双方 response 合并时使用更弱语义：任一 Ignore → Ignore；否则任一 Overlap → Overlap；只有双方都是 Block → Block。单向规则只能把 Block 降级为 Ignore，不能把 Overlap 升级为 Block。

### `CollisionDetectionMode`（当前存在）

- `Discrete`：只检查当前世界形状；
- `Continuous`：使用 previous/current origin 检查帧内首次命中。

首版 Continuous 只承诺 AABB/AABB 与 AABB/Tile。涉及 Circle 时记录受控诊断并使用离散检测，不得假装返回可靠 TOI。

### `PassThroughDirection` / `OneWayCollision`（当前存在）

描述“另一个对象允许相对此 Collider 从哪些世界方向穿过”。`tolerance` 用于 previous side 判断和接触抖动容差。

该配置只提供意图；最终决定必须同时读取双方 previous/current origin、相对位移和接触法线。它不应由窄检直接消费，而应由 response 策略处理。

### `Collider`（当前存在，注册生命周期已落地）

**目的**：持久保存形状、过滤、响应和检测意图。

**所有权**：由 GameObject/Provider 拥有。PhysicsWorld 借用并在注册时分配 ID。

**不变量**：

- 注册后的 ID 有效且在世界生命周期内唯一；
- 注册期间不得移动 Collider 容器导致借用地址失效；
- tag 仅调试，不参与规则；
- enabled=false 时不参与检测和查询；
- 改 shape/filter/response 后最迟在下一固定步生效。

**不负责**：owner Transform、Body 状态、接触缓存、事件监听。

### `ColliderProvider`（当前存在）

把一个 GameObject 的零个或多个持久 Collider 暴露给 Scene/PhysicsWorld。Provider 仍然拥有底层容器；返回 span 的内存必须在注册期稳定。

接口同时提供 mutable 与 const span。Provider 不分配全局 ID，也不注册自己。

## 4. 接触、命中与目标身份

### `CollisionPair`（当前结构化契约）

当前保存两个 `CollisionTarget`。普通 Collider 宽相候选另用 `BroadPhasePair { ColliderId first, second }`；接触与事件 pair 不给 Tile 伪造 ColliderId。

### `CollisionManifold`（当前存在）

保存与形状类型无关的几何结果：从 first 指向 second 的单位法线、非负穿透深度、最多两个接触点。离散命中的 TOI 为 1；连续命中由 `CollisionHit` 保存 `[0,1]` TOI。

Manifold 不保存 response、Body 或事件阶段。

### `CollisionHit`（当前存在）

窄检输出。只回答“几何上是否命中、在哪里、何时命中”，不决定 Ignore/Overlap/Block。`time_of_impact` 必须有限并限制在 `[0,1]`。

### `CollisionOverlap` / `CollisionContact`（当前存在，目标统一）

当前分别表达 overlap 和带 response 的 contact。目标设计保留 `CollisionContact` 作为本步规范化事实，并由 response 区分 Block/Overlap；Gameplay 兼容事件可以继续使用 `CollisionOverlap` 视图。

### `TileCoordinate`（当前存在）

有符号 `(x, y)` 网格坐标。必须支持负数以表达越界查询和非零世界原点附近候选。排序规则为 y 优先、x 次之。

### `CollisionTargetKind` / `CollisionTarget`（当前存在）

统一标识普通 Collider 或当前 Tile World 的一个 Tile：

- Collider target：有效 collider ID；
- Tile target：有效 TileCoordinate，collider ID 必须无效。

提供构造工厂、有效性检查、相等和稳定排序。不得同时把两个分支都标为有效。

### `CollisionEventPhase` / `CollisionEvent`（当前存在）

phase 为 Begin、Stay、End。事件保存规范化 target pair、最后有效 manifold 和 response。End 可以沿用上一缓存 manifold，调用方不得把 End 的 penetration 当成本步仍重叠。

### `CollisionFrame`（当前存在，内容生成待实现）

一个固定物理步的临时结果容器，当前保存 contacts 和 events，并由 `clear()` 复用容量。它由 PhysicsWorld 每步复用，不能跨步向外暴露内部 span；views 和 candidate pairs 保持为 CollisionSystem 内部临时数据。

### `ContactCache`（目标新增）

拥有上一固定步和当前固定步的规范化 contact map/set。负责比较并生成 Begin/Stay/End，不负责检测或 Gameplay 路由。

禁用、注销、Tile World 更换和 teleport 必须使相关 key 失效。缓存 key 必须包含完整 `CollisionTarget`，不能只按地图统一 ID。

## 5. 策略与 CollisionSystem

### `ColliderView`（当前存在，目标扩展）

固定步内对 Collider 的只读快照，包含 Collider 引用、target/owner handle、previous/current owner origin，以及便于算法读取的 Body 引用或索引。View 不拥有任何对象，仅在本固定步有效。

### `BroadPhaseProxy` / `BroadPhasePair` / `IBroadPhaseIndex`（当前存在）

`IBroadPhaseIndex` 是可维护状态的普通 Collider 空间索引。`synchronize` 接收全量 proxy 快照；`collect_pairs` 输出规范化候选；`query_aabb` 为查询和局部检索提供候选；`clear` 清除索引。实现只能跨帧保存 ColliderId 与 bounds，不能保存 `Collider*` 或 `ColliderView*`。四叉树、Sweep-and-Prune 和 Brute Force 都可作为独立实现注入。

### `ICollisionDetectionStrategy`（当前存在）

对一个已经规范化的 pair 做几何检测。离散与连续实例可以共享几何辅助函数，但不能持有场景对象所有权。

### `ICollisionResponseStrategy`（当前存在）

合并双方 response，处理 one-way 和 runtime 临时忽略规则，输出最终 Ignore/Overlap/Block。它选择语义但不直接移动对象。

### `BruteForceBroadPhaseIndex`（后续目标）

首版正确性基线。遍历 `i < j` 的所有普通 Collider view，做 enabled、filter 和包围盒快速排除后输出 pair。即使未来增加 Sweep-and-Prune，它仍应保留作测试 oracle。

### `DefaultDiscreteCollisionStrategy`（目标新增）

实现 AABB/AABB、Circle/Circle、AABB/Circle。负责法线方向规范化、退化情况和接触点，不负责 response。

### `SweptAabbCollisionStrategy`（目标新增）

实现 previous/current AABB 的相对扫掠和 AABB/Tile TOI。首版遇到 Circle 时明确回退离散策略。

### `DefaultCollisionResponseStrategy`（目标新增）

实现 response 合并、one-way 和 drop-through predicate。临时忽略数据由调用方以只读上下文传入，策略不能拥有 gameplay binding。

### `CollisionSystem`（当前阶段边界已落地，算法待实现）

**目的**：协调 view 构建、普通候选收集、Tile 候选、窄检、响应选择、Block 求解和本步 contact 输出。

**拥有**：一个 `IBroadPhaseIndex`、两个 detection strategy 和一个 response strategy。

**不拥有**：GameObject、Body、Collider、Tile World、跨步 ContactCache、Gameplay listener。

当前公开入口为 `evaluate(span<ColliderView>, tile_world, fixed_dt, CollisionFrame&)`，本轮只清空 frame。事件生命周期将归 PhysicsWorld/ContactCache，不由 CollisionSystem 持有跨步状态。

## 6. PhysicsWorld 与配置

### `PhysicsWorldConfig`（当前存在）

保存固定步长、单次 advance 最大步数、求解迭代数和世界重力。构造 world 时校验一次，运行期间首版保持不变。

默认 gravity 为零，具体游戏必须显式配置向下重力；这样 top-down 游戏不会被意外施加平台游戏重力。

### `PhysicsObjectHandle`（当前存在）

轻量、可比较的 world 内注册句柄，包含单调递增 generation/value。无效值为零。它标识注册 entry，不等同于 ColliderId，也不等同于 ActorId。

### `PhysicsWorld`（当前生命周期骨架已落地）

**目的**：每 Scene 的物理运行时与唯一协调者。

**拥有**：

- 配置与 accumulator；
- 注册 entries 和索引；
- ID/handle 计数器；
- `PhysicsSystem`、`CollisionSystem`、`ContactCache`、`CollisionFrame`；
- pending 注册/注销/状态同步队列；
- 核心 collision listeners 的非 owning 注册集合。

**借用**：GameObject、Provider、Body、Collider、当前 `ITileCollisionWorld`。

**实现**：`ICollisionQueryService`。

**不负责**：加载 Tile Map、创建 GameObject、Team、攻击和伤害。

**完成标准**：在固定输入下可独立运行；注册/注销安全；Block、Overlap、Tile、查询和事件共享同一世界事实。

## 7. Tile Map 类型

### `TileCollisionType`（当前存在）

- `Empty`：无碰撞；
- `Block`：普通阻挡；
- `Overlap`：只产生重叠；
- `OneWay`：使用 cell 的 OneWayCollision 规则。

### `TileOutOfBoundsPolicy`（当前存在）

- `Block`：地图外视为不可进入的实心区域，默认；
- `Empty`：地图外不产生碰撞。

### `TileCollisionCell`（当前存在）

保存 type、filter、one_way 和调试 tag。它是查询返回值，不拥有 Tile 数据。Empty cell 的其他字段不应影响结果。

### `ITileCollisionWorld`（当前存在）

物理核心读取规则网格的最小接口：origin、tile_size、columns、rows、out_of_bounds_policy、cell_at。它不提供渲染纹理、图块 ID、房间、对象层或地图生成 API。

### `TileCollisionResolver`（目标新增）

无状态或仅持有临时缓冲区的算法组件。负责 world/tile 转换、移动 AABB 候选范围、Tile rect、Tile 窄检和查询；不拥有适配器。

项目中的 `TileMapCollisionAdapter` 应实现 `ITileCollisionWorld`，把项目 Tile 属性转换成 `TileCollisionCell`。

## 8. Query 类型

### `RayCastQuery`（当前存在）

描述 origin、direction、max_distance 和 filter。目标实现将 direction 规范化；零方向、非有限输入或非正距离返回无命中。

### `SegmentCastQuery`（当前存在）

描述 start、end 和 filter。零长度 segment 返回无命中，不自动变成 point overlap 查询。

### `CollisionQueryHit`（当前存在，目标调整）

目标把 `collider` 字段替换为 `CollisionTarget target`，继续保存 point、normal、distance、fraction。最近命中按 fraction，平局按 target 稳定顺序决定。

### `ICollisionQueryService`（当前存在）

纯只读查询契约。PhysicsWorld 实现它；查询不得推进模拟、修改 contact cache 或产生 Gameplay 事件。

## 9. PhysicsService

### `CollisionStrategyFactories`（当前存在）

保存四个创建函数。每次应用必须创建互不共享的策略实例。所有工厂都成功返回后才能替换目标系统，保持强异常安全。

### `PhysicsService`（当前存在）

全局配置门面，不是世界。它只负责：一次性接受完整 factory 集、为 CollisionSystem 创建策略、查询配置状态和 shutdown。

目标接入点是在 Scene/PhysicsWorld 初始化时调用 `apply_to`。它不得保存注册对象、Tile World、contact cache 或当前 Scene 指针。

## 10. Gameplay 碰撞类型

### ID、`ColliderRole` 与 `TeamRelation`（当前存在）

ActorId、TeamId、AttackInstanceId、AttackDefinitionId 属于 Gameplay 身份。它们与 ColliderId 分离是必要设计。Role 决定事件路由；TeamRelation 决定业务资格，不参与几何检测。

### `ColliderBinding`（当前存在）

把 Collider 映射到 owner Actor、Team 和 Role。一个 Collider 同时只能有一个普通 binding。

### `HitBoxBinding`（当前存在）

在普通 binding 基础上补充真正的攻击责任人、攻击实例和定义。其内嵌 binding 的 role 必须为 HitBox。

### `ActorCollisionRig`（当前存在）

原子描述一个 Actor 的 Body、PushBox、HurtBox 和 Sensor 集合。runtime 应验证所有非零 Collider 已注册且不重复，再一次性提交。

### Gameplay Event（当前 phase/target 契约已落地）

- `BodyContactEvent`：Body 与世界/普通 Collider 的接触；
- `PushBoxOverlapEvent`：两个 PushBox；
- `HitOverlapEvent`：定向的 HitBox → HurtBox。

目标事件都应带 `CollisionEventPhase`；Body 的 other 应升级为 `CollisionTarget` 以表示 Tile。

### `GameplayCollisionListener`（当前存在）

业务接收接口。默认空函数允许只覆写关心的事件。listener 是借用关系，runtime 不负责删除；事件分发期间的 add/remove 必须延迟到分发结束。

### `TeamRelationResolver`（当前存在）

项目提供的纯查询策略。不得按 TeamId 数值大小猜测关系；相同 Team 是否 Friendly 也由实现明确决定。

### `IGameplayCollisionRuntime`（当前存在）

Service 转发目标。声明 binding、解绑和 drop-through 操作。目标实现可扩展 listener 注册、攻击实例结束和对象生命周期清理接口。

### `GameplayCollisionRuntime`（目标新增）

每 Scene 的具体 runtime，拥有 binding map、rig map、命中去重集合和 drop-through 集合，借用 PhysicsWorld、TeamRelationResolver 与 listeners。它消费核心事件并输出语义事件，不执行伤害。

### `GameplayCollisionService`（当前存在）

全局无状态门面，只保存一个 active runtime 非 owning 指针。当前 attach/detach 身份检查、无 runtime 日志和转发行为应保留。它不得变成全局 binding 数据库。

## 11. 类型覆盖索引

| 分组 | 当前公开类型覆盖位置 |
| --- | --- |
| Body | §2 |
| Shape / Collider / Filter / OneWay | §3 |
| Pair / Manifold / Hit / Contact / Overlap | §4 |
| View / Strategy / CollisionSystem | §5 |
| PhysicsWorld 目标类型 | §6 |
| Tile 目标类型 | §7 |
| Query | §8 |
| PhysicsService / factories | §9 |
| Gameplay IDs / bindings / events / listener / runtime / service | §10 |

下一篇：[逐函数实现契约](04-function-responsibilities.md)
