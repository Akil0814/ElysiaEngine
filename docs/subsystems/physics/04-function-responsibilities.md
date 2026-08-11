# 04｜逐函数实现契约

返回：[物理文档入口](README.md)　上一篇：[逐类职责](03-class-responsibilities.md)　下一篇：[碰撞算法](05-collision-algorithms.md)

## 1. 通用约定

下文的“目标签名”是实现阶段建议 API，不代表当前仓库已经存在。除明确说明外：

- 所有函数在主线程、Scene 物理阶段调用；
- 借用指针/引用只在所属对象仍注册且未销毁时有效；
- 只读查询不推进模拟、不产生事件；
- 浮点输入必须先检查 `std::isfinite`；
- 输出集合由调用者传入时，函数应追加还是清空必须明确。首版统一为**清空后写入完整结果**，函数名使用 `build`/`collect` 时也遵守该规则；
- 稳定输出在返回前按规范化 target/pair 排序并去重。

## 2. 现有小型纯函数

### `operator|(PassThroughDirection, PassThroughDirection)`（当前已实现）

- **调用者**：Collider 配置代码。
- **职责**：按位合并两个方向，不修改参数。
- **返回**：包含双方 bit 的新枚举值。
- **无效输入**：枚举底层出现未知 bit 时仍按位保留；运行时 one-way 判断只消费已知 bit。
- **测试**：Up|Left 同时可被查询，输入保持不变。

### `operator|=(PassThroughDirection&, PassThroughDirection)`（当前已实现）

- **职责**：把第二个方向合并到第一个并返回第一个引用。
- **副作用**：只修改 first。
- **测试**：链式组合和重复组合幂等。

### `has_pass_through_direction(directions, direction)`（当前已实现）

- **职责**：判断 direction 的全部 bit 是否包含在 directions 中。
- **特殊规则**：查询 `None` 永远返回 false，防止空 bit 被误判为“包含”。
- **测试**：单 bit、多 bit、None、未包含方向。

## 3. Provider 函数

### `PhysicsBodyProvider::physics_body()` / const overload（当前接口）

- **调用者**：Scene 注册和 PhysicsWorld 状态同步。
- **时机**：对象加入 Scene、固定步安全点、调试查询。
- **职责**：返回该对象唯一 Body 的借用指针；对象没有 Body 时可以返回 nullptr。
- **生命周期**：非空指针在对象注册期内地址稳定。
- **不得做**：创建 Body、推进模拟、隐式注册。
- **测试**：mutable/const overload 指向同一对象；无 Body 安全返回 nullptr。

### `ColliderProvider::colliders() const`（当前接口）

- **职责**：返回所有 Collider 的连续只读视图。
- **不变量**：span 内 ID 不重复；注册期不改变容器地址或长度。
- **空集合**：合法，不应注册 collider 索引。

### `ColliderProvider::colliders() noexcept`（目标新增 overload）

- **调用者**：`PhysicsWorld::register_object`。
- **职责**：允许 world 为 invalid Collider 写入稳定 ID，并在明确 API 下调整 enabled 等 runtime 字段。
- **不得做**：返回临时 vector 的 span。
- **测试**：ID 写入后 const overload 可见同一值。

## 4. PhysicsWorld 函数

### 建议公开接口

```cpp
class PhysicsWorld final : public ICollisionQueryService
{
public:
    explicit PhysicsWorld(PhysicsWorldConfig config = {});

    bool configure_strategies(const PhysicsService& service);
    PhysicsObjectHandle register_object(
        elysia::core::GameObject& owner,
        PhysicsBodyProvider* body_provider,
        ColliderProvider* collider_provider);
    bool unregister_object(PhysicsObjectHandle handle) noexcept;

    bool set_tile_world(const ITileCollisionWorld& world) noexcept;
    bool clear_tile_world(const ITileCollisionWorld& world) noexcept;

    std::uint32_t advance(double frame_delta_seconds);
    void reset() noexcept;

    std::optional<CollisionQueryHit> raycast(
        const RayCastQuery& query) const override;
    std::optional<CollisionQueryHit> segment_cast(
        const SegmentCastQuery& query) const override;
};
```

### `PhysicsWorld(config)`（目标）

- **调用者**：Scene 构造或成员初始化。
- **职责**：校验并保存 config，初始化空注册表、无 Tile World、零 accumulator 和从 1 开始的 handle/Collider ID 计数器。
- **校验**：fixed delta 有限且大于零；max steps 和 solver iterations 大于零；gravity 分量有限。
- **失败**：无效配置抛 `std::invalid_argument`，不能静默修正，因为这属于开发期配置错误。
- **测试**：默认构造、每个无效字段、初始空状态。

### `configure_strategies(service)`（目标）

- **调用者**：Scene 初始化，仅一次。
- **职责**：调用 service 为内部 CollisionSystem 安装完整独立策略集。
- **返回**：安装成功 true；service 未配置或工厂产物为空 false。
- **异常**：工厂异常原样传播，内部 CollisionSystem 保持原策略不变。
- **重复调用**：首版允许显式重新安装，但必须仍保持全套原子替换；生产 Scene 通常只调用一次。

### `register_object(owner, body_provider, collider_provider)`（目标）

- **调用者**：`Scene::register_scene_object_interfaces`。
- **时机**：物理步之外立即注册；步中或事件中进入 pending queue。
- **输入**：owner 必须存活；两个 provider 至少一个非空。
- **步骤**：
  1. 查 owner 索引；已注册则返回已有 handle；
  2. 读取 Body 和 mutable Collider span；
  3. 验证 Body 参数、Collider 形状、span 地址稳定前提和非零 ID 冲突；
  4. 预留 handle 与所有新 Collider ID，但验证失败时不提交；
  5. 写回新 ID；
  6. 一次性插入 entry、owner index、collider index；
  7. current/previous origin 都初始化为 owner 当前 position，防止首次注册产生虚假 CCD。
- **返回**：有效 handle；失败返回 invalid handle 并记录 collision 类别诊断。
- **异常安全**：容器分配失败时不留下半注册索引；已经写回但未提交的 ID 必须恢复 invalid。
- **确定性**：按 provider span 顺序分配递增 ID。
- **测试**：Body-only、Collider-only、两者都有、空 provider、重复 owner、重复 ID、部分失败回滚、事件期间注册。

### `unregister_object(handle)`（目标）

- **输入**：有效且属于当前 world 的 handle。
- **步骤**：步外立即或步中排队；移除 collider/owner 索引；清相关 contact；使 handle 无效；不删除 owner/provider。
- **返回**：找到或成功排队 true；无效/未知 handle false。
- **ID**：已释放 Collider ID 不复用，也不强制把对象字段清零；对象若进入另一个 world，注册流程应为其重新分配并显式覆盖旧世界 ID。
- **事件**：正常禁用/注销可对仍有效的另一方生成 End；world reset/Scene 退出不分发 End。
- **测试**：幂等性、步中注销、旧 handle 不能控制新 entry、cache 清理。

### `set_tile_world(world)`（目标）

- **职责**：绑定唯一活动 Tile World 非 owning 引用。
- **校验**：tile size 有限且两轴大于 epsilon；columns/rows 非负；origin 有限。
- **规则**：无绑定时成功；绑定同一实例幂等；已有不同实例时返回 false，要求先 clear。
- **状态**：绑定成功后使查询立即可见，模拟在下一固定步读取。
- **测试**：无效尺寸、同一实例重复绑定、冲突绑定。

### `clear_tile_world(world)`（目标）

- **职责**：身份匹配后解除绑定，并清除所有 Tile target 接触缓存。
- **返回**：匹配并清除 true；没有绑定或实例不匹配 false。
- **事件**：正常运行期间清除可产生 Tile End；Scene reset/退出走静默 reset。
- **生命周期**：adapter 销毁前必须成功 clear。

### `advance(frame_delta_seconds)`（目标）

- **调用者**：非暂停的 `Scene::on_update`，每渲染帧一次。
- **职责**：可变帧时间转固定物理步。
- **规则**：非有限或非正 delta 返回 0 且不改 accumulator；执行步数不超过配置；超额完整步丢弃并计诊断；保留不足一步余数。
- **副作用**：可能移动对象、更新速度、产生事件、应用 pending 操作。
- **返回**：实际执行固定步数量。
- **重入**：执行期间再次调用必须拒绝并记录错误。
- **测试**：不足一步、恰好一步、多步、超过上限、NaN、负数、暂停时未调用。

### `fixed_step(dt)`（目标私有）

- **调用者**：只由 advance。
- **前置**：dt 等于配置 fixed delta；不允许重入。
- **严格顺序**：应用 pending → 清理失效 entry → snapshot previous → integrate → build views → collect/detect → solve → 写 contact cache → build/dispatch events → clear forces → 应用事件期间产生的 pending。
- **失败策略**：单个无效 entry 记录诊断并跳过；核心容器分配异常可以传播，但必须解除 stepping guard。
- **测试**：使用 fake system/listener 验证调用顺序。

### `reset()`（目标）

- **职责**：静默清除 entries、indices、pending、cache、frame buffers、Tile 借用和 accumulator；保留 config 与已安装策略。
- **不得做**：向 Gameplay 分发 End、删除 owner、调用 Scene API。
- **ID**：计数器不回绕；reset 后可以从 1 重启仅因为所有旧 handles 已随 world 生命周期失效。若同一 world 对外 handle 可能残留，则继续递增更安全，首版采用继续递增。

### `raycast(query)` / `segment_cast(query)`（目标实现）

- **调用者**：Gameplay、AI、调试工具。
- **职责**：查询当前已提交的 world 状态，包括 enabled 普通 Collider 与活动 Tile World，返回最近合法命中。
- **过滤**：使用 query filter 与目标 filter 的双向规则；Ignore response 仍由项目决定是否可查询，首版 Ignore 不命中，Overlap/Block 均可命中。
- **平局**：fraction 相同（epsilon 内）时按 CollisionTarget 稳定顺序。
- **副作用**：无；不能修改 contact cache、accumulator 或事件。
- **测试**：见 §11。

## 5. PhysicsSystem 函数

### 建议接口

```cpp
void integrate(std::span<PhysicsEntry> entries,
               const PhysicsWorldConfig& config,
               double fixed_delta_seconds) const;
void clear_forces(std::span<PhysicsEntry> entries) const noexcept;
```

### `integrate(entries, config, dt)`（目标，替代当前空 `step`）

- **输入**：已清理、owner/Body 借用有效的 entry span。
- **Static**：不改 velocity、force 或 position；force 在步尾统一清除。
- **Kinematic**：不消费重力和 force；按 `velocity * dt` 更新 position，再应用分轴限速。
- **Dynamic**：
  1. 验证 mass；
  2. `acceleration = gravity * gravity_scale + accumulated_force / mass`；
  3. 半隐式欧拉 `velocity += acceleration * dt`；
  4. 线性阻尼乘以 `max(0, 1 - damping * dt)`；
  5. 按 max_speed 每轴 clamp；
  6. `position += velocity * dt`。
- **禁用或 inactive/destroyed**：跳过，不写任何状态。
- **无效 Body 参数**：记录诊断并跳过该 entry，禁止传播 NaN。
- **确定性**：按 handle 升序遍历。
- **测试**：三种 Body、重力、力、阻尼、限速、禁用、无效质量、固定输入复现。

### `clear_forces(entries)`（目标）

- **职责**：对仍有效的所有 Body 把 accumulated_force 设为 zero；包括本步禁用、Static 和 Kinematic Body，避免旧力在重新启用/改类型后突然生效。
- **异常**：noexcept。

### 当前 `PhysicsSystem::step(body_entries, delta)`

当前函数只 `(void)` 输入。实现阶段不应直接在该模板里塞入世界、Tile 和事件依赖；应由上述明确 entry 类型与 `PhysicsWorld` 协调接口替换。迁移完成前可保留薄包装调用 `integrate`，之后删除模板入口。

## 6. CollisionSystem 与策略函数

### Strategy setter（当前已实现）

`set_broad_phase_strategy`、`set_discrete_detection_strategy`、`set_continuous_detection_strategy`、`set_response_strategy`：

- 接管 `unique_ptr` 所有权；
- 允许传 nullptr 清空槽；
- noexcept，仅替换对应槽；
- 不应隐式重置其他槽或世界状态；
- 测试所有权转移和独立槽。

### Strategy getter（当前已实现）

四个 getter：

- 返回内部策略的 const 非 owning 指针；
- 空槽返回 nullptr；
- noexcept；
- 指针只在对应 setter、CollisionSystem 移动/销毁前有效。

### `IBroadPhaseStrategy::collect_pairs`（当前接口）

- **输入**：本固定步有效的普通 ColliderView span。
- **输出**：先清空 out_pairs，再写规范化候选。
- **最低过滤**：空/无效/disabled、自身 pair、相同 owner 按 group 规则可跳过；准确 filter 可在系统统一函数处理以避免策略不一致。
- **不得做**：Tile、精确 manifold、response、事件。
- **测试**：N=0/1/2、多对象、重复 ID、稳定顺序、不漏重叠包围盒。

### `ICollisionDetectionStrategy::detect`（当前接口）

- **输入**：first/second 已规范化且有效；dt 为固定步。
- **返回**：几何命中返回 CollisionHit，否则 nullopt。
- **法线**：严格 first → second。
- **不得做**：修改 Collider/owner、检查 Team、分发事件。
- **测试**：各形状组合及交换顺序后的法线翻转。

### `ICollisionResponseStrategy::resolve`（当前接口）

- **输入**：两 view、已验证 hit、fixed dt；目标扩展应增加只读 response context 以查询临时忽略对。
- **返回**：最终 Ignore/Overlap/Block。
- **步骤**：filter 已在之前完成；合并 response；若 Block 再检查 one-way/drop-through。
- **不得做**：位置修正或修改 ignore set。

### `CollisionSystem::build_views(entries, out_views)`（目标）

- **职责**：从注册表生成本步只读 ColliderView；跳过 destroyed/inactive/disabled Collider；验证 ID 索引一致。
- **世界形状**：可在 view 中缓存 previous/current AABB/circle，或由统一 helper 延迟计算；同一固定步不能混用不同来源。
- **排序**：ColliderId 升序。

### `collect_pairs(views, out_pairs)`（目标）

- **职责**：调用 broad phase；规范化、排序、去重；统一应用 filter。
- **无策略**：记录一次配置错误并返回空集合，不崩溃。

### `detect_contacts(views, pairs, tile_world, dt, out_contacts)`（目标）

- **职责**：为普通 pair 选择 discrete/continuous detector；由 TileCollisionResolver 增加 Tile candidates；对 hit 调用 response 策略；丢弃 Ignore；输出规范化 contact。
- **模式选择**：任一 Collider 为 Continuous 且组合受支持时用 continuous；否则 discrete。
- **Tile**：只查询移动包围盒覆盖的格子，不遍历全图。
- **输出**：按 target pair 排序；同 pair 多 hit 保留最早 TOI，离散平局保留更深/稳定法线的结果。

### `solve_contacts(entries, contacts, iterations, dt)`（目标）

- **职责**：只处理 Block；按逆质量修正 position 和 closing normal velocity；每次迭代按稳定 pair 顺序。
- **零总逆质量**：不修正，仍保留 contact。
- **Overlap**：绝不改状态。
- **Tile**：Tile 逆质量始终为零。
- **输出后处理**：若迭代改变位置，需要更新后续算法使用的 current world shape。

### `build_events(contacts, cache, out_events)`（目标）

- **职责**：把本步 contacts 提交给 cache，生成 Begin/Stay/End，稳定排序。
- **同一步重复 contact**：合并后再进入 cache。
- **End manifold**：来自 previous cache。

### 当前 `dispatch_events(entries, delta)`

当前为空壳且名称混合了检测与事件。迁移期可以作为 `PhysicsWorld` 内部兼容包装；目标实现完成后删除，避免 Scene 绕过 fixed-step coordinator。

## 7. PhysicsService 函数

### `configure(factories)`（当前已实现）

- 未配置且四个 callable 都非空时接管 factories 并返回 true；
- 已配置或任一 callable 为空返回 false，保持旧状态；
- 不在 configure 时调用 factory；
- 目标行为保持不变。

### `apply_to(collision_system)`（当前已实现）

- 未配置返回 false；
- 依次创建四个临时策略；
- 任一产物为空则返回 false，目标系统保持不变；
- 工厂异常传播，目标系统保持不变；
- 全部成功后再移动进目标系统；
- 每次调用创建独立实例。

### `is_configured()`（当前已实现）

只返回配置标志，noexcept，无副作用。

### `shutdown()`（当前已实现）

清空 factory 和配置标志，noexcept；不遍历或修改已经创建的 PhysicsWorld/CollisionSystem。

## 8. Tile Collision 函数

### `ITileCollisionWorld` 建议接口

```cpp
virtual core::Vector2 world_origin() const noexcept = 0;
virtual core::Vector2 tile_size() const noexcept = 0;
virtual int columns() const noexcept = 0;
virtual int rows() const noexcept = 0;
virtual TileOutOfBoundsPolicy out_of_bounds_policy() const noexcept = 0;
virtual TileCollisionCell cell_at(TileCoordinate coordinate) const noexcept = 0;
```

### `world_origin()` / `tile_size()`

- 返回世界空间左上角和单格尺寸；
- 在一次 fixed_step 或 query 内必须保持稳定；
- size 两轴必须有限且大于 epsilon。

### `columns()` / `rows()`

- 返回非负网格尺寸；
- 0 表示空地图；
- 不包含越界虚拟 Block 行列。

### `out_of_bounds_policy()`

返回越界采样是 Block 还是 Empty。一次 fixed_step 内保持稳定。

### `cell_at(coordinate)`

- 范围内把项目 Tile 数据映射为 TileCollisionCell；
- 越界时返回与 policy 一致的虚拟 cell；
- noexcept，不记录每格日志；
- 不暴露项目 Tile 引用，避免借用失效。

### `TileCollisionResolver::world_to_tile(world, point)`（目标）

- 使用 `floor((point-origin)/tile_size)` 分轴计算；
- 返回有符号坐标；
- 不能用向零截断；
- world 配置无效时返回 nullopt。

### `tile_rect(world, coordinate)`（目标）

返回 `origin + coordinate * size` 的世界 AABB。允许传越界坐标以构造虚拟边界格。

### `candidate_range(world, swept_bounds)`（目标）

- 最小边使用 floor；最大边使用 `floor((max-origin-epsilon)/size)`，保持半开边界；
- Empty 越界策略可裁剪到地图范围；Block 策略保留实际覆盖的越界坐标，但必须限制为 swept bounds 覆盖的有限范围；
- 空/非有限 bounds 返回空 range。

### `collect_contacts(...)`（目标）

- 遍历 candidate range，跳过 Empty 和 filter 不匹配 cell；
- 为每格构造 Tile target 与 world rect；
- 根据 body mode 选择离散或 swept AABB；
- 处理 OneWay；
- 输出稳定排序并去重的 contact；
- 不修改项目 Tile Map。

## 9. Query 函数细则

### `raycast(query)`

1. 验证 origin/direction/max_distance；
2. 规范化 direction，保留 max_distance；
3. 遍历 broad phase 提供的 ray candidates；首版可遍历全部注册 Collider；
4. AABB 使用 slab，Circle 使用二次方程；
5. Tile 使用 2D DDA 或有限候选范围，推荐 DDA；
6. 过滤无效/disabled/Ignore/不匹配目标；
7. 选 fraction 最小者；平局按 target；
8. normal 指向射线来源一侧，distance 为世界距离，fraction=`distance/max_distance`。

起点在形状内部时返回 distance=0、fraction=0；normal 使用与射线方向相反的稳定主轴/归一化方向。

### `segment_cast(query)`

- `delta=end-start`；长度小于等于 epsilon 返回 nullopt；
- 内部转换为 normalized ray + segment length；
- 返回 fraction 相对 `[start,end]`；
- 其他语义与 raycast 一致。

## 10. Gameplay runtime 与 Service 函数

### `GameplayCollisionService::attach_runtime(runtime)`（当前已实现）

- 无 active runtime 时保存借用指针并返回 true；
- 同一 runtime 重复 attach 幂等 true；
- 不同 runtime 冲突时日志并返回 false；
- 不接管 runtime 所有权。

### `detach_runtime(runtime)`（当前已实现）

- 无 active runtime 返回 false；
- 身份不匹配时日志并保持原 runtime；
- 匹配时清空并返回 true；
- runtime 析构前必须调用。

### `has_active_runtime()`（当前已实现）

返回指针是否非空；不验证借用对象是否仍存活，因此生命周期必须由 Scene 保证。

### `bind_actor` / `bind_collider` / `bind_hit_box` / `unbind_collider`（当前转发）

- 使用 `runtime_or_log` 获取 active runtime；
- 无 runtime 返回 false；
- 有 runtime 时原样转发并返回结果；
- Service 不重复业务校验，具体 runtime 负责完整一致性。

### `request_drop_through(request)`（当前部分实现）

- Service 先拒绝无效、相同 actor/target ID；
- 目标设计把 target 升级为 `CollisionTarget`，允许普通单向 Collider 或 Tile；
- runtime 再验证 actor 是已绑定 Body、target 当前是有效支撑面、规则允许穿透；
- 成功后只忽略这一对，不得禁用 actor 对所有平台的碰撞。

### `runtime_or_log(operation)`（当前已实现）

- 有 runtime 返回借用指针；
- 无 runtime 记录 collision ERROR，消息包含 operation，返回 nullptr；
- noexcept，不抛异常。

### `IGameplayCollisionRuntime` 五个当前纯虚函数

- `bind_actor`：原子验证并提交 rig，同时创建所有反向索引；失败不保留部分 binding。
- `bind_collider`：验证 Collider 已在 PhysicsWorld 注册、ID 未绑定、Actor/Team/Role 有效。
- `bind_hit_box`：先验证普通 binding role=HitBox，再验证 instigator 与 attack IDs，原子提交。
- `unbind_collider`：移除普通/HitBox binding、从 rig 集合和命中缓存清除引用；未知 ID 返回 false。
- `request_drop_through`：验证当前支撑关系并增加临时忽略 key。

### `GameplayCollisionRuntime::add_listener(listener)` / `remove_listener(listener)`（目标）

- listener 非 owning；重复 add 幂等；未知 remove 返回 false；
- 分发期间修改进入 pending listener queue；
- 稳定使用注册顺序分发；
- listener 析构前必须 remove。

### `on_collision_event(event)`（目标）

- **调用者**：PhysicsWorld 核心事件分发。
- **职责**：查询双方 binding，按 role 规范化方向，检查 team/attack 去重，然后构造相应 Gameplay event。
- **Begin**：Body、PushBox、Hit 均可路由；Hit 只在 Begin 尝试新增命中。
- **Stay**：Body/PushBox 路由；Hit 默认不重复。
- **End**：Body/PushBox 路由并清支撑/drop-through；Hit 用于清瞬时状态但不撤销伤害。
- **不得做**：直接调用伤害系统；只通知 listener。

### `end_attack_instance(id)`（目标）

删除该 attack instance 的命中去重集合和已结束 HitBox binding；无效/未知 ID 返回 false。攻击生命周期必须显式结束，不能按帧自动猜测。

### `update_drop_through(current_contacts)`（目标私有）

每固定步事件处理后检查忽略 pair。只要 actor 仍与目标几何相交或尚未完全越过平台容差就保留；完全离开后删除。对象/Tile World 注销时立即清理。

### `GameplayCollisionListener` 三个当前回调

- `on_body_contact`：接收规范化 Body 为主体的接触；默认无操作。
- `on_push_box_overlap`：接收稳定排序但语义对称的两个 PushBox；默认无操作。
- `on_hit_overlap`：始终 hit_box 为攻击方、hurt_box 为受击方；默认无操作。

目标事件增加 phase。回调允许请求对象销毁/解绑，但实际 registry 修改延迟到分发结束。

### `TeamRelationResolver::relation(source, target)`（当前纯虚）

- 输入必须是有效 TeamId；runtime 在调用前校验；
- noexcept、只读、无副作用；
- 返回 Friendly/Neutral/Hostile；
- 不得假设数值排序；
- 相同 team 的关系由项目实现定义并测试。

## 11. Query 和事件测试索引

| 函数族 | 必测场景 |
| --- | --- |
| register/unregister | 重复、冲突、回滚、步中延迟、ID 不复用 |
| advance/fixed_step | 0/负/NaN、小于一步、恰好一步、8+ 步、重入 |
| integrate/clear_forces | 三 BodyType、重力、阻尼、限速、禁用、无效质量 |
| collect/detect/solve | 稳定 pair、三形状组合、Block/Overlap、零逆质量 |
| contact cache | Begin→Stay→End、disable、unregister、Tile clear |
| tile helpers | 负坐标、非零 origin、半开边界、非正方形格子、越界策略 |
| ray/segment | AABB、Circle、Tile、inside、平局、过滤、无效输入 |
| gameplay binding | 原子提交、冲突、解绑清理、role 规范化 |
| gameplay event | Body/PushBox/Hit、phase、team、命中去重、回调期间注销 |

下一篇：[碰撞算法与数值约定](05-collision-algorithms.md)
