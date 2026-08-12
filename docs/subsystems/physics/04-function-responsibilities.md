# 04｜逐函数实现契约

> **实现状态**：本章描述的首版算法已经落地。最终 API 使用 `PhysicsObjectState`、`CollisionShapeView`、`CollisionStrategySet` 和 `ICollisionResponseStrategy::classify`；force 在 `integrate` 读取后立即清零。文中旧 `PhysicsBodyView`、`ColliderView`、独立 setter、`resolve`、`PhysicsService` 与“空操作”说明仅保留为迁移记录，不再是可调用 API。

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

### `ColliderProvider::colliders() noexcept`（当前接口）

- **调用者**：`PhysicsWorld::register_object`。
- **职责**：允许 world 为 invalid Collider 写入稳定 ID，并在明确 API 下调整 enabled 等 runtime 字段。
- **不得做**：返回临时 vector 的 span。
- **测试**：ID 写入后 const overload 可见同一值。

## 4. PhysicsWorld 函数

### 当前公开接口（节选）

```cpp
class PhysicsWorld final : public ICollisionQueryService
{
public:
    explicit PhysicsWorld(PhysicsWorldConfig config = {});
    PhysicsWorld(PhysicsWorldConfig config, CollisionStrategySet strategies);

    PhysicsObjectHandle register_object(
        elysia::core::GameObject& owner,
        PhysicsBodyProvider* body_provider,
        ColliderProvider* collider_provider);
    bool unregister_object(PhysicsObjectHandle handle);

    bool set_tile_world(const ITileCollisionWorld& world) noexcept;
    bool clear_tile_world(const ITileCollisionWorld& world) noexcept;

    std::uint32_t advance(double frame_delta_seconds);
    void reset() noexcept;

    void set_debug_capture(PhysicsDebugCapture capture) noexcept;
    PhysicsDebugCapture debug_capture() const noexcept;
    const PhysicsDebugSnapshot& debug_snapshot() const noexcept;

    std::optional<CollisionQueryHit> raycast(
        const RayCastQuery& query) const override;
    std::optional<CollisionQueryHit> segment_cast(
        const SegmentCastQuery& query) const override;
    void raycast_all(const RayCastQuery&,
        std::vector<CollisionQueryHit>& out_hits) const override;
    void segment_cast_all(const SegmentCastQuery&,
        std::vector<CollisionQueryHit>& out_hits) const override;
    void overlap_aabb(const AabbOverlapQuery&,
        std::vector<CollisionOverlapQueryHit>& out_hits) const override;
    void overlap_circle(const CircleOverlapQuery&,
        std::vector<CollisionOverlapQueryHit>& out_hits) const override;
    std::optional<CollisionQueryHit> sweep_aabb(
        const AabbSweepQuery&) const override;
};
```

### `PhysicsWorld(config)`（当前已实现）

- **调用者**：Scene 构造或成员初始化。
- **职责**：校验并保存 config，初始化空注册表、无 Tile World、零 accumulator 和从 1 开始的 handle/Collider ID 计数器。
- **校验**：fixed delta 有限且大于零；max steps、solver/CCD iterations 和 Tile 单次候选上限大于零；gravity 与全部数值容差有限且落在合法范围。
- **失败**：无效配置抛 `std::invalid_argument`，不能静默修正，因为这属于开发期配置错误。
- **测试**：默认构造、每个无效字段、初始空状态。

### `PhysicsWorld(config, strategies)`（当前已实现）

- **调用者**：测试、性能实验或自定义空间索引的 Scene。
- **职责**：一次性接管完整 `CollisionStrategySet`。
- **异常**：任一策略为空时构造失败，不产生部分配置 World。
- **默认路径**：单参数构造自动调用 `make_default_collision_strategies()`，无需启动配置。

### `register_object(owner, body_provider, collider_provider)`（当前已实现）

- **调用者**：`Scene::register_scene_object_interfaces`。
- **时机**：步外立即提交；步中/事件回调立即预留稳定 handle/ID，并在下一安全边界提交。
- **输入**：owner 必须存活；两个 provider 至少一个非空。
- **步骤**：
  1. 查 owner 索引；已注册则返回已有 handle；
  2. 读取 Body 和 mutable Collider span；
  3. 验证实际至少取得一个 Body 或 Collider，并拒绝任何预填 Collider ID；Body 参数和形状校验留给后续算法阶段；
  4. 预留 handle 与所有新 Collider ID，但验证失败时不提交；
  5. 写回新 ID；
  6. 一次性插入 entry、owner index、collider index；
  7. current/previous origin 都初始化为 owner 当前 position，防止首次注册产生虚假 CCD。
- **返回**：有效 handle；失败返回 invalid handle 并记录 collision 类别诊断。
- **异常安全**：容器分配失败时不留下半注册索引；已经写回但未提交的 ID 必须恢复 invalid。
- **确定性**：按 provider span 顺序分配递增 ID。
- **测试**：Body-only、Collider-only、两者都有、空 provider、相同 provider 幂等、不同 provider 冲突、预填 ID、失败不写入 ID。

### `unregister_object(handle)`（当前已实现）

- **输入**：有效且属于当前 world 的 handle。
- **步骤**：步外立即移除 collider/owner 索引，把 Provider 中全部 Collider ID 写回 invalid；步中/回调中进入 pending queue；不删除 owner/provider。
- **返回**：找到或成功排队 true；无效/未知 handle false。
- **ID**：已释放 Collider ID 不复用，并强制把对象字段清零；对象只有在旧 World 注销后才能进入另一 World。
- **事件**：注销静默清相关 contact，不生成 End；world reset/Scene 退出同样静默。
- **测试**：幂等性、旧 handle 不能控制新 entry、ID 清零与数值不复用。

### `set_tile_world(world)`（当前已实现）

- **职责**：绑定唯一活动 Tile World 非 owning 引用。
- **校验**：tile size 有限且两轴大于 epsilon；columns/rows 非负；origin 有限。
- **规则**：无绑定时成功；绑定同一实例幂等；已有不同实例时返回 false，要求先 clear。
- **状态**：绑定成功后使查询立即可见，模拟在下一固定步读取。
- **测试**：无效尺寸、同一实例重复绑定、冲突绑定。

### `clear_tile_world(world)`（当前已实现）

- **职责**：身份匹配后解除绑定，并清除所有 Tile target 接触缓存。
- **返回**：匹配并清除 true；没有绑定或实例不匹配 false。
- **事件**：正常运行期间清除可产生 Tile End；Scene reset/退出走静默 reset。
- **生命周期**：adapter 销毁前必须成功 clear。

### `advance(frame_delta_seconds)`（当前已实现）

- **调用者**：非暂停的 `Scene::on_update`，每渲染帧一次。
- **职责**：可变帧时间转固定物理步。
- **规则**：非有限或非正 delta 返回 0 且不改 accumulator；执行步数不超过配置；每一步在进入 `fixed_step` 前先消费 accumulator；超额完整步丢弃并以饱和计数诊断；保留不足一步余数。
- **副作用**：执行完整积分、检测、求解、缓存和事件流程；查询与 Debug Capture 开关不改变该结果。
- **返回**：实际执行固定步数量。
- **重入**：执行期间再次调用必须拒绝并记录错误。
- **测试**：不足一步、恰好一步、多步、超过上限、NaN、负数、暂停时未调用。

### `fixed_step(dt)`（当前完整流程）

- **调用者**：只由 advance。
- **前置**：dt 等于配置 fixed delta；不允许重入。
- **顺序**：应用安全边界操作 → snapshot previous/current → integrate（同时消费 force）→ 构造值语义 shape views → evaluate/solve → 提交 Transform/velocity → ContactCache 生成事件 → listener 批次分发 → 清理已完全分离的 drop-through。
- **失败策略**：单个无效 entry 记录诊断并跳过；核心容器分配异常或 listener 异常可以传播，但必须解除 stepping/dispatch guard。listener 不得抛异常，物理层不回滚已提交状态；Application update boundary 负责记录 `UnhandledException` 并 FaultExit。
- **测试**：使用 fake system/listener 验证调用顺序。

### `reset()`（当前已实现）

- **职责**：静默清除 entries、indices、pending、cache、frame buffers、Tile 借用和 accumulator；保留 config 与已安装策略。
- **时机**：步外立即执行；advance 或事件回调期间仅设置 pending reset。当前整批 listener 快照完成后优先执行 reset，并停止本次 advance 的后续固定步；其他 pending 操作被一并清除。
- **不得做**：向 Gameplay 分发 End、删除 owner、调用 Scene API。
- **ID**：计数器不回绕；reset 后可以从 1 重启仅因为所有旧 handles 已随 world 生命周期失效。若同一 world 对外 handle 可能残留，则继续递增更安全，首版采用继续递增。

### `set_debug_capture(capture)` / `debug_capture()` / `debug_snapshot()`（当前已实现）

- **调用者**：Scene 调试适配或独立诊断工具；必须在 `advance` 前设置当前帧需要的分类。
- **职责**：裁剪未知位、保存 Shapes/BroadPhase/Contacts/Velocities 掩码，并在掩码改变时立即清空旧快照。
- **默认**：`None`；不请求采集时 `CollisionSystem` 收到空输出，不创建或复制任何调试 vector 数据。
- **隔离**：capture 不改变积分、候选、窄检、求解、事件或 `PhysicsStepStats`；快照只用于诊断。
- **生命周期**：返回的快照引用归 World 所有，下一固定步、capture 修改或 reset 都可能改变内容。
- **测试**：默认空、逐分类采集、分类切换清空、关闭清空，以及开启/关闭前后物理统计和 contact 一致。

### `raycast(query)` / `segment_cast(query)`（当前已实现）

- **调用者**：Gameplay、AI、调试工具。
- **行为**：复用对应 all-hits 函数并返回第一个元素；无合法命中返回 `std::nullopt`。
- **过滤**：使用 query filter 与目标 filter 的双向规则；Ignore response 仍由项目决定是否可查询，首版 Ignore 不命中，Overlap/Block 均可命中。
- **排序**：all-hits 按 distance、target 排序并按 target 去重；最近查询严格等于第一项。
- **副作用**：无；不能修改 contact cache、accumulator 或事件。
- **测试**：见 §11。

## 5. PhysicsSystem 函数

### 建议接口

```cpp
void integrate(std::span<PhysicsObjectState> entries,
               const PhysicsWorldConfig& config,
               double fixed_delta_seconds) const noexcept;
```

### `integrate(entries, config, dt)`（当前已实现）

- **输入**：已清理、owner/Body 借用有效的 entry span。
- **Static**：不改 velocity 或 position；进入函数后本步旧 force 仍会被清除。
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

### force 清理（已合并到 `integrate`）

- `integrate` 先复制并清空 accumulated_force，再执行本步积分。这样事件回调中新施加的 force 不会在步尾被误删，而会留给下一固定步。

### 已删除的 `PhysicsSystem::step(body_entries, delta)`

旧模板入口已经删除，不保留兼容包装。Scene 只能通过 `PhysicsWorld::advance` 进入固定步流程，World 内部调用类型化 `integrate`。

## 6. CollisionSystem 与策略函数

### `CollisionStrategySet` 与构造（当前已实现）

四个策略作为一个完整值传入 `CollisionSystem` 构造函数。任一 `unique_ptr` 为空都抛出 `std::invalid_argument`，因此运行时不存在“漏配某个 detector”的状态。

### Strategy getter（当前已实现）

四个 getter：

- 返回内部策略的 const 非 owning 引用；
- noexcept；
- 引用只在 CollisionSystem 移动/销毁前有效。

### `IBroadPhaseIndex`（当前接口）

- `synchronize(span<const BroadPhaseProxy>)` 接收本固定步全量快照；实现可按 ColliderId 在内部增删改 proxy。
- `collect_pairs(vector<BroadPhasePair>&)` 必须先清空输出，再写入规范化、稳定排序且去重的候选。
- `query_aabb(bounds, vector<ColliderId>&)` 同样先清空输出，再返回稳定排序且去重的候选 ID。
- `clear() noexcept` 清除全部索引状态。
- 索引只能长期保存 ColliderId、bounds 和必要过滤快照，不能保存跨帧 `CollisionShapeView*`、`Collider*` 或 GameObject 指针。
- 当前提供 Brute Force 与 SAP；SAP 是默认实现。

### `ICollisionDetectionStrategy::detect`（当前接口）

- **输入**：first/second 已规范化且有效；`CollisionDetectionContext` 携带 fixed dt 和 World 统一 epsilon。
- **返回**：几何命中返回 CollisionHit，否则 nullopt。
- **法线**：严格 first → second。
- **不得做**：修改 Collider/owner、检查 Team、分发事件。
- **测试**：各形状组合及交换顺序后的法线翻转。

### `ICollisionResponseStrategy::classify`（当前接口）

- **输入**：两个 `CollisionShapeView`、已验证 hit 和只读 `CollisionResponseContext`。
- **返回**：最终 Ignore/Overlap/Block。
- **步骤**：filter 已在之前完成；合并 response；若 Block 再检查 one-way/drop-through。
- **不得做**：位置修正或修改 ignore set。

### `CollisionSystem::build_views(entries, out_views)`（目标）

- **职责**：从注册表生成本步值语义 `CollisionShapeView`；跳过 destroyed/inactive/disabled Collider；验证 ID 索引一致。
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

### CollisionSystem 内部 velocity/position solve（当前已实现）

- **当前职责**：只处理 Block；按逆质量修正 position，并以稳定 pair 顺序执行法向与切向顺序冲量。
- **材质**：摩擦取双方几何平均，弹性取较大值；低于 restitution threshold 的接触强制零弹性。
- **静摩擦**：完全抵消所需切向冲量未超过 `static_friction * normal_impulse` 时采用所需值；否则按 dynamic friction 截断。
- **Kinematic**：逆质量为零但速度参与相对速度，因此可通过摩擦携带 Dynamic。
- **零总逆质量**：不修正，仍保留 contact。
- **Overlap**：绝不改状态。
- **Tile**：Tile 逆质量始终为零。
- **输出后处理**：若迭代改变位置，需要更新后续算法使用的 current world shape。

### `ContactCache::update(contacts, out_events)`（当前已实现）

- **职责**：把本步 contacts 提交给 cache，生成 Begin/Stay/End，稳定排序。
- **同一步重复 contact**：合并后再进入 cache。
- **End manifold**：来自 previous cache。

### `CollisionSystem::evaluate(...)`（当前已实现）

入口接收 mutable `PhysicsObjectState`、只读 `CollisionShapeView`、Tile World、临时忽略对、config、fixed dt、输出 frame、stats、`PhysicsDebugCapture` 和可空 debug snapshot。它同步索引、收集候选、执行检测/分类/CCD/迭代求解并稳定输出 contact；debug 输出为空或分类未请求时不得复制、排序或去重诊断数据。跨步事件仍由 PhysicsWorld 的 ContactCache 构造。

## 7. 默认策略构造函数

### `make_default_collision_strategies()`（当前已实现）

返回 SAP、默认离散检测、Swept AABB 和默认响应的完整独立实例集。

### `make_brute_force_collision_strategies()`（当前已实现）

返回 Brute Force 与相同三个默认策略，用作测试 oracle 和小规模诊断。两个 builder 每次都创建互不共享的实例。

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

### checked Tile coordinate/range helpers（当前已实现）

- 使用 `floor((point-origin)/tile_size)` 分轴计算；
- 返回有符号坐标；
- 不能用向零截断；
- world 配置无效、结果超出 `int` 可表示范围时返回 nullopt；
- 候选数乘法先做溢出检查，再与 `max_tile_candidates_per_operation` 比较；超过上限整段拒绝并计入 `rejected_tile_candidate_ranges`；
- 模拟候选使用半开最大边，overlap/sweep 查询使用包含边界相切目标的闭合模式。

### `tile_rect(world, coordinate)`（目标）

返回 `origin + coordinate * size` 的世界 AABB。允许传越界坐标以构造虚拟边界格。

### `candidate_range(world, swept_bounds)`（当前已实现）

- 最小边使用 floor；最大边使用 `floor((max-origin-epsilon)/size)`，保持半开边界；
- Empty 越界策略可裁剪到地图范围；Block 策略保留实际覆盖的越界坐标，但必须限制为 swept bounds 覆盖的有限范围；
- 空/非有限 bounds、不可表示坐标、候选数量溢出或超过配置上限返回空 range；不得截断执行。

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

### `raycast_all` / `segment_cast_all`

- 进入函数先清空输出 vector；无效输入保持空；
- Collider 使用 AABB slab/Circle 二次方程，Tile 使用完整距离 DDA；
- Ignore 不命中，Overlap/Block 均返回；
- 统一按 distance、target 排序，同一 target 只保留最近命中；
- 不写 ContactCache、事件、stats 或 Debug Snapshot。

### `overlap_aabb` / `overlap_circle`

- 查询当前已提交世界形状，支持 AABB、Circle 与 Tile；
- manifold normal 始终从 query 指向 target；
- Tile 候选使用 floor、包含相切目标的闭合最大边、非零 origin、负坐标和非方形格；
- 结果按 target 排序去重；输出前总是清空。

### `sweep_aabb`

- 使用 start AABB 与 displacement 构造 swept bounds，复用 Swept AABB；
- 只检测 AABB Collider 和 Tile，明确跳过 Circle；
- 初始重叠为 TOI 0；零位移仍走 AABB-only sweep/initial-overlap 路径，并选 target 最小者，Circle 目标仍被排除；
- 返回最近 fraction，平局按 target。

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

### `bind_actor` / `bind_collider` / `bind_hit_box` / `unbind_actor` / `unbind_collider`（当前转发）

- 使用 `runtime_or_log` 获取 active runtime；
- 无 runtime 返回 false；
- 有 runtime 时原样转发并返回结果；
- Service 不重复业务校验，具体 runtime 负责完整一致性。

### `request_drop_through(request)`（当前已实现）

- Service 先拒绝无效、相同 actor/target ID；
- target 使用 `CollisionTarget`，允许普通单向 Collider 或 Tile；
- runtime 再验证 actor 是已绑定 Body、target 当前是有效支撑面、规则允许穿透；
- 成功后只忽略这一对，不得禁用 actor 对所有平台的碰撞。

### `runtime_or_log(operation)`（当前已实现）

- 有 runtime 返回借用指针；
- 无 runtime 记录 collision ERROR，消息包含 operation，返回 nullptr；
- noexcept，不抛异常。

### `IGameplayCollisionRuntime` 当前生命周期函数

- `bind_actor`：拒绝 invalid owner/team、空 rig、重复 Collider 和 role 冲突；异常安全地提交 rig 与全部反向索引，失败不保留部分 binding。
- `bind_collider`：验证 Collider 已在 PhysicsWorld 注册、ID 未绑定、Actor/Team/Role 有效；拒绝 HitBox，HitBox 必须走专用入口。
- `bind_hit_box`：验证 role=HitBox、instigator 与全部 attack ID，原子提交普通/HitBox 两个索引。
- `unbind_actor`：一次删除 Actor rig、该 owner 的全部普通/HitBox binding，以及以该 Actor 为 hurt owner 的命中历史；其他 Actor 和 attack instance 保持不变。
- `unbind_collider`：移除普通/HitBox binding、从 rig 集合和命中缓存清除引用；未知 ID 返回 false。
- `request_drop_through`：验证当前支撑关系并增加临时忽略 key。

### `GameplayCollisionRuntime::add_listener(listener)` / `remove_listener(listener)`（当前已实现）

- listener 非 owning；重复 add 幂等；未知 remove 返回 false；
- 分发期间修改进入 pending listener queue；
- 稳定使用注册顺序分发；
- listener 析构前必须 remove。

### `on_collision_event(event)`（当前已实现）

- **调用者**：PhysicsWorld 核心事件分发。
- **职责**：先复制双方 binding、HitBox、instigator rig Team，并把当前核心事件能产生的全部语义事件构造成值列表；随后使用当前 listener 快照分发。回调内 unbind Actor/Collider、clear runtime 或 end attack 不得使当前事件迭代器失效。
- **Begin**：Body、PushBox、Sensor 均可路由；Hit 只在 Begin 尝试新增命中。
- **Stay/End**：Body、PushBox、Sensor 路由；Hit 默认不重复。
- **Sensor**：仅 Sensor↔Body 且物理 response=Overlap；字段始终按 sensor、body 排列，Team 与攻击去重不参与。
- **Hit Team**：使用 instigator 对应 Actor rig 的 Team；找不到有效 rig 时拒绝命中，不信任 HitBox collider 自带 Team。
- **不得做**：直接调用伤害系统；只通知 listener。

### `end_attack_instance(id)`（目标）

删除该 attack instance 的命中去重集合和已结束 HitBox binding；无效/未知 ID 返回 false。攻击生命周期必须显式结束，不能按帧自动猜测。

### `update_drop_through(current_contacts)`（目标私有）

每固定步事件处理后检查忽略 pair。只要 actor 仍与目标几何相交或尚未完全越过平台容差就保留；完全离开后删除。对象/Tile World 注销时立即清理。

### `GameplayCollisionListener` 四个当前回调

- `on_body_contact`：接收规范化 Body 为主体的接触；默认无操作。
- `on_push_box_overlap`：接收稳定排序但语义对称的两个 PushBox；默认无操作。
- `on_hit_overlap`：始终 hit_box 为攻击方、hurt_box 为受击方；默认无操作。
- `on_sensor_overlap`：始终 sensor 在前、body 在后；Begin/Stay/End 均转发。

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
| integrate/force consumption | 三 BodyType、重力、阻尼、限速、禁用、无效质量 |
| collect/detect/solve | 稳定 pair、三形状组合、Block/Overlap、零逆质量 |
| contact cache | Begin→Stay→End、disable、unregister、Tile clear |
| tile helpers | 负坐标、非零 origin、半开边界、非正方形格子、越界策略 |
| ray/segment | nearest/all-hits、AABB、Circle、Tile、inside、格角、排序、过滤、无效输入 |
| overlap/sweep | AABB/Circle/Tile、相切、零位移、初始重叠、Circle sweep 跳过、只读性 |
| gameplay binding | 原子提交、冲突、解绑清理、role 规范化 |
| gameplay event | Body/PushBox/Hit/Sensor、phase、team、命中去重、回调期间注销 |

下一篇：[碰撞算法与数值约定](05-collision-algorithms.md)
