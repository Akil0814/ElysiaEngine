# 物理与 Gameplay 碰撞框架

## 当前状态

本目录记录 Moonline 的物理与 Gameplay 碰撞框架边界。当前阶段提供可编译的数据契约、可注入的碰撞策略槽，以及由 `Scene` 调用的系统入口，尚未实现：

- 运动积分、重力和阻尼；
- 粗检测、形状相交、连续检测和空间索引；
- 阻挡、穿透修正和接地判断；
- PushBox 推挤；
- HitBox/HurtBox 配对、命中去重和伤害结算。

`Scene` 会自动登记实现 `PhysicsBodyProvider` 或 `ColliderProvider` 的 `GameObject`，并在非暂停帧依次调用 `PhysicsSystem::step` 和 `CollisionSystem::dispatch_events`。这两个入口目前不会改变对象或产生碰撞事件。

## 分层边界

### `engine/physics`

物理核心只认识 Collider、几何形状、过滤位、响应类型和接触数据，不认识 Actor、玩家、敌人、阵营、攻击或伤害。

- `ColliderId` 是 Collider 的稳定标识；零值表示无效 ID。
- `CollisionFilter::category` 表示 Collider 所属类别，`mask` 表示候选对象类别，`group` 为后续成组过滤预留。
- `CollisionResponse` 描述 Ignore、Overlap 或 Block 意图。
- `CollisionDetectionMode` 描述 Collider 使用离散或连续检测的意图。
- `CollisionManifold` 使用法线、穿透深度和接触点表达与形状无关的几何结果。
- `CollisionHit::time_of_impact` 为归一化帧区间内的命中时刻。
- `Collider::tag` 仅用于调试，不参与碰撞规则。

`CollisionResponse::Overlap` 是 Collider 表达重叠或触发语义的唯一方式。物理核心不提供独立的 trigger 标志或旧事件兼容接口。

具体 category 位由使用物理核心的上层模块定义。物理核心不得加入 Player、Enemy、HitBox 等固定类别。

### 形状与策略

持久 Collider 当前只声明两种局部空间形状：

- `AabbShape`：相对 GameObject 原点的轴对齐矩形；
- `CircleShape`：相对 GameObject 原点的圆心和非负半径。

`ColliderShape` 使用 `std::variant` 保存形状，不提供旧的矩形字段或形状继承层。Capsule、旋转矩形和多边形不属于当前契约。

`Collider::one_way` 是可选的单向通过配置；无值表示普通碰撞，有值时 `PassThroughDirection` 可用 `|` 组合允许通过的世界轴方向。`Up` 表示另一个 Collider 可以相对此 Collider 从下向上通过。单向配置只修饰原本为 `Block` 的响应，不改变 `Ignore` 或 `Overlap`；第一版只承诺 AABB 语义，具体判断算法尚未实现。

未来的单向判断必须同时使用双方 previous/current origin 的相对位移和接触法线，不能只检查某个对象自己的移动方向。`OneWayCollision::tolerance` 默认值为 `0.01f`，当前只声明非负契约，不做运行时校验或修正。

碰撞流水线预留三个由 `CollisionSystem` 独占持有的策略槽：

- `IBroadPhaseStrategy`：从帧内 `ColliderView` 生成候选 `CollisionPair`；
- discrete `ICollisionDetectionStrategy`：处理普通离散检测；
- continuous `ICollisionDetectionStrategy`：处理需要 previous/current origin 的连续检测；
- `ICollisionResponseStrategy`：在命中后根据双方 Collider、manifold 和帧时间选择最终 Ignore、Overlap 或 Block。

策略槽默认允许为空，当前 `dispatch_events` 不调用策略。检测策略生成的 manifold 法线统一从 `CollisionPair::first` 指向 `second`，供响应策略进行稳定的方向判断。BruteForce、Sweep-and-Prune、AABB/Circle 相交、Swept AABB 和单向响应都属于后续具体实现。分步 AABB 属于 PhysicsSystem 的时间步进策略，不作为 Collider 检测模式。

### PhysicsService

`PhysicsService` 是全局策略配置入口，不是全局物理世界。它只保存 BroadPhase、Discrete、Continuous 和 Response 四个策略工厂，并能为任意 `CollisionSystem` 安装一组独立策略实例。

- 四个工厂必须一次性完整配置；
- 配置成功后保持不变，直到显式 `shutdown()`；
- 每次 `apply_to()` 都创建新的策略实例，不在 Scene 之间共享状态；
- 任一工厂返回空指针时不修改目标 `CollisionSystem`；
- 工厂异常继续向调用者传播，目标系统保持原状。

当前 PhysicsService 不保存 Collider、PhysicsBody、CollisionFrame 或场景生命周期状态，也尚未自动接入 Scene/Application。具体策略实现和应用启动配置完成后，再由基础 Scene 统一应用 Service 配置。

### 查询契约

`RayCastQuery` 和 `SegmentCastQuery` 只描述最近命中查询；`ICollisionQueryService` 是纯接口，当前没有实现。Ray 与 Segment 是瞬时查询，不属于 `ColliderShape`，也不能作为零厚度 Block Collider。

### `engine/gameplay/collision`

Gameplay 便利层把通用 Collider 绑定为 Actor 相关语义：

- `ColliderBinding`：Collider 所属 Actor、Team 和 Role；
- `ActorCollisionRig`：一个 Actor 的 Body、PushBox、HurtBox 和 Sensor 集合；
- `HitBoxBinding`：HitBox 的 owner、instigator、攻击实例和攻击定义；
- `TeamRelationResolver`：由项目决定两个 Team 是 Friendly、Neutral 还是 Hostile；
- `GameplayCollisionListener`：预留 Body、PushBox 和命中事件入口；
- `IGameplayCollisionRuntime`：由具体 Scene runtime 实现 binding、解绑和临时穿透请求；
- `GameplayCollisionService`：全局无状态门面，只保存当前 runtime 的非 owning 指针并转发调用。

`GameplayCollisionService` 不保存 Collider、binding 或临时忽略碰撞对。没有 active runtime 时，业务调用记录 `collision` 类别错误日志并返回 false；挂载不同 runtime 会被拒绝，卸载时执行身份检查，防止旧场景清除新场景的 runtime。本阶段只提供显式 attach/detach，不接入 Scene、SceneManager 或 Application。

`DropThroughRequest` 只指定 actor 与当前支撑平台的 ColliderId。未来 runtime 应只临时忽略这一对 Collider，并在 actor 完全离开目标平台后恢复；宽限时间、支撑追踪和恢复算法本轮不实现。

`teams::Player`、`teams::Enemy` 和 `teams::Neutral` 是常用预设，不限制项目创建更多 `TeamId`。敌对关系不能通过 Team 数值直接推断，必须由 `TeamRelationResolver` 决定。

## 来源与目标

物理接触本身没有攻击方向，只包含一对 Collider。Gameplay 层按 Role 将事件规范化：

```text
Body     <-> World    -> BodyContactEvent
PushBox  <-> PushBox  -> PushBoxOverlapEvent
HitBox   ->  HurtBox  -> HitOverlapEvent
```

普通 Collider 的来源由 `ColliderBinding::owner` 表示。攻击的责任来源由 `HitBoxBinding::instigator` 表示；这允许飞行道具拥有自己的 Collider，同时把命中归属给发射它的 Actor。`attack_instance` 用于未来的一次攻击内命中去重，`attack_definition` 用于查找招式数据。

过滤和语义判断分两阶段进行：

1. 物理核心根据 category/mask 产生可能接触的 Collider 对。
2. Gameplay 层查询 binding 和 Team 关系，将 Collider 对路由到 Body、PushBox 或战斗处理器。

## 后续实现顺序

1. 实现 AABB/Circle 世界坐标转换和 BruteForce 粗检测策略。
2. 实现 AABB/AABB、Circle/Circle 和 AABB/Circle 离散检测策略。
3. 实现 PhysicsBody 积分以及 Body 与世界的阻挡、接地和墙体处理。
4. 按实际高速物体需求实现连续检测和 Ray/Segment 查询服务。
5. 实现 Gameplay binding、PushBox、HitBox/HurtBox 路由和命中去重。
6. 在项目 `gameplay` 层实现伤害、硬直、击退和具体角色规则。

每个阶段都应先补齐单元测试，再接入下一个阶段；不得把 Gameplay 语义下沉到 `engine/physics`。
