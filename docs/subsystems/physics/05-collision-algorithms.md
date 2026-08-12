# 05｜碰撞算法与数值约定

> **实现状态**：本章约定已用于首版实现；SAP 为默认宽相，Brute Force 为 oracle。Circle CCD 仍按约定回退离散。

> 本章既是当前首版实现的数值契约，也是后续扩展必须保持的兼容规则。

返回：[物理文档入口](README.md)　上一篇：[逐函数实现契约](04-function-responsibilities.md)　下一篇：[Tile Map 碰撞](06-tile-map-collision.md)

## 1. 坐标、边界和 epsilon

- 世界 X 向右、Y 向下；
- `Rect::top()` 数值小于 `bottom()`；
- Collider 的 AABB/Circle 均是 owner origin 的局部形状；
- 几何命中把边界相切视为 penetration=0 的接触；Tile 模拟候选范围仍使用半开边界，避免把只位于下一格边界的 Tile 重复采样；
- overlap/sweep 查询使用包含相切目标的闭合 Tile 候选范围；碰撞求解和查询不要共用一个“是否包含边界”的无名函数；
- detection strategy 统一接收 `CollisionDetectionContext`，其 epsilon 来自 `PhysicsWorldConfig::collision_epsilon`；需要更大的 penetration slop 时在 solver config 中独立命名，不能散落魔法数。

所有算法入口先拒绝非有限坐标、尺寸、半径、速度和 dt。NaN 不能进入排序比较，否则会破坏确定性。

## 2. 世界形状构造

### AABB

```cpp
world_rect = shape.local_rect.translated(owner_origin);
```

previous/current 分别使用 view 中对应 origin。局部 rect 尺寸为空时形状无效。

### Circle

```cpp
world_center = owner_origin + shape.local_center;
radius = shape.radius;
```

radius 小于等于 epsilon 时视为空。首版没有 owner rotation 或 scale，因此局部圆不需要变换半径。

### Swept bounds

粗检连续对象时，使用 previous world AABB 与 current world AABB 的 merged rect。Circle 的粗检 AABB 是 `center ± radius`；即使 Circle 窄检首版离散，粗检也应正确覆盖当前形状。

## 3. Pair 和法线规范

普通 Collider pair 总是：

```cpp
first  = min(id_a, id_b);
second = max(id_a, id_b);
```

所有 manifold normal 从 first 指向 second。交换输入形状顺序时，接触点可以相同，但法线必须取反。

普通 Collider 与 Tile 的 key 固定让 Collider target 在前、Tile target 在后，因此法线从对象指向 Tile。两个 Tile 不互相检测。

法线必须近似单位向量。退化情况下不能返回 zero normal 给 Block solver；应使用稳定 fallback：

1. 优先使用相对位移的最大绝对轴；
2. 再使用相对速度的最大绝对轴；
3. 都为零时按 pair 稳定顺序选择 `(1,0)`。

## 4. CollisionFilter

建议集中为一个纯函数，所有 broad phase、query 和 Tile 路径复用：

```cpp
bool should_collide(const CollisionFilter& a,
                    const CollisionFilter& b) noexcept;
```

顺序：

1. 若 `a.group != 0 && a.group == b.group`：正 group 返回 true，负 group 返回 false；
2. 否则要求 `(a.mask & b.category) != 0`；
3. 同时要求 `(b.mask & a.category) != 0`。

category=0 通常不会与任何依赖 mask 的对象匹配。默认 Collider 的 category=0 因而是“尚未分类”，不是“属于所有类别”。

Query filter 与目标 filter 也使用同一双向规则。若项目希望 query 只做单向 mask，应增加明确命名的独立规则，不能悄悄改变 `should_collide`。

## 5. Brute Force 粗检

首版算法：

```text
for i in [0, n):
  for j in [i+1, n):
    检查 view 有效、Collider enabled、ID 不同
    检查 filter
    计算双方 broad-phase AABB
    若 AABB 相交或任一 continuous swept bounds 相交，输出 pair
排序、去重
```

注意：

- 粗检可以多报，不能漏报；
- 同一 owner 的 Collider 不应无条件跳过，因为 HurtBox/HitBox 可能需要项目策略；是否自碰由 group/filter 配置；
- response=Ignore 可以提前跳过，但 Query 规则仍独立；
- Brute Force 是正确性 oracle。未来实现 Sweep-and-Prune 时，用相同 views 比较两者候选包含关系。

## 6. AABB/AABB 离散检测

对 world rect `a`、`b`：

先确认 `touches_or_intersects`。随后计算把 A 沿正/负方向推出 B 的四个边界距离：

```text
positive_x_depth = a.right  - b.left
negative_x_depth = b.right  - a.left
positive_y_depth = a.bottom - b.top
negative_y_depth = b.bottom - a.top
depth_x = min(positive_x_depth, negative_x_depth)
depth_y = min(positive_y_depth, negative_y_depth)
```

这样在 A 完全包含 B、B 完全包含 A 或同中心时仍得到真实最小平移距离，而不是只得到交集宽高。边界相切时 depth 为 0，仍返回接触。选择最小推出轴：

- `depth_x <= depth_y`：X 法线，平局稳定选择 X；
- `depth_y < depth_x`：Y 法线。

法线符号由中心差决定。例如 `b.center.x >= a.center.x` 时 X 法线为 `(1,0)`。

方向由所选轴两侧 depth 的较小者决定；完全平局时以中心差决定符号，中心也相同则稳定使用 `+X`（X 轴未选时为 `+Y`）法线。contact point 取对应推出边与另一轴重叠区间的中心。penetration 为所选轴 depth，TOI=1。

## 7. Circle/Circle 离散检测

```text
delta = center_b - center_a
radius_sum = radius_a + radius_b
distance_sq = dot(delta, delta)
```

若 `distance_sq > radius_sum²` 则无命中；相切返回 penetration=0。命中时：

- distance > epsilon：normal=`delta/distance`；
- 中心重合：使用稳定 fallback normal；
- penetration=`radius_sum-distance`；
- contact point 可取 `center_a + normal * (radius_a - penetration/2)`；
- TOI=1。

不得直接对零距离做除法。

## 8. AABB/Circle 离散检测

### 圆心在矩形外

把 circle center 分轴 clamp 到 rect 得到 closest point。`delta = circle_center - closest`。若 `dot(delta,delta) >= radius²` 则无穿透；否则 normal 是 rect → circle 的 `delta` 方向，penetration=`radius-distance`，contact point=closest。

### 圆心在矩形内

closest 会等于 center，不能用 zero delta。计算圆心到 left/right/top/bottom 四边距离，选择最短边；平局按 X 后 Y 的稳定顺序。normal 指向该边外侧，penetration=`radius + distance_to_edge`，contact point 在所选边上。

如果规范 pair 中 Circle 在 first、AABB 在 second，复用 AABB→Circle 算法后翻转 normal。

## 9. Swept AABB

目标是计算 first 与 second 在 `[0,1]` 固定步区间内的首次接触。使用相对位移：

```text
relative_move = (current_first - previous_first)
              - (current_second - previous_second)
```

把 second 视为静止，对每轴计算进入/离开距离和时间：

- relative_move 轴分量为零且轴投影不重叠：无命中；
- 分量为零且投影重叠：该轴 entry=-∞、exit=+∞；
- 非零时按运动方向选择近边/远边并除以相对位移。

```text
entry_time = max(x_entry, y_entry)
exit_time  = min(x_exit, y_exit)
```

满足以下条件才命中：

- entry_time <= exit_time；
- exit_time >= 0；
- entry_time 在 `[0,1]`，允许 epsilon；
- previous AABB 未穿透。若初始穿透，改用离散 manifold，TOI=0。

法线来自产生最大 entry time 的轴，与 relative_move 方向相反。轴时间平局使用与离散算法一致的稳定规则。

首版响应至少把对象移动到 TOI，再处理剩余法向速度。若采用先积分后回退方案，必须保证最终 Transform 不越过碰撞面，且不会把切向位移也抹掉。

## 10. Circle 连续检测限制

首版 `Continuous` Collider 若涉及 Circle：

- broad phase 使用 swept bounding AABB，避免完全跳过；
- narrow phase 使用 current 状态离散检测；
- 第一次遇到该组合时记录限频 diagnostic；
- 文档和测试不得声称它能防止高速穿透。

后续可实现 swept circle/circle 与 circle/AABB，但不能通过加大 Circle 或无限子步冒充准确 CCD。

## 11. Response 合并

在 geometry hit 后按以下顺序：

1. pair/filter 已不匹配：Ignore；
2. 任一 Collider response=Ignore：Ignore；
3. 任一 response=Overlap：Overlap；
4. 双方 Block：继续；
5. 临时 drop-through key 命中：Ignore；
6. one-way 判断允许穿过：Ignore；
7. 否则 Block。

若双方都配置 one-way，分别评估双方作为平台的规则；任一规则明确允许穿过即 Ignore。首版建议禁止动态对象配置双向复杂 one-way，并记录诊断。

## 12. 单向平台判断

以 platform 为配置 `OneWayCollision` 的一方，actor 为另一方。不能只检查 actor.velocity。

以“允许从下向上穿过 Up”为例，Y 向下坐标系中：

- actor previous top/bottom 与 platform previous bottom/top 决定之前在哪一侧；
- 相对移动 `actor_move - platform_move` 必须朝允许方向；
- manifold normal 必须与平台阻挡面一致；
- previous 时 actor 已在允许穿过侧，或在 tolerance 内；
- actor 从阻挡侧落向平台时仍然 Block。

四个方向应通过统一的轴、符号和边界选择表实现，避免复制四套 if。测试必须包含移动平台，因为仅看 actor 自己的 velocity 会得出错误结果。

## 13. Block 求解

### 逆质量

```text
Static/Kinematic/Tile: inv_mass = 0
Dynamic: inv_mass = 1 / mass
```

质量无效的 Dynamic 在进入求解前应被诊断并跳过积分；其约束速度也必须视为零，不得让非法 Body 的旧速度制造“幽灵推动”。Static、Collider-only 和 disabled Body 的约束速度为零；只有 Kinematic 以零逆质量保留 authored velocity。

### 位置修正

设 normal 从 A 指向 B，penetration 为正：

```text
correction = normal * max(penetration - slop, 0) * correction_percent
sum = inv_mass_a + inv_mass_b
A.position -= correction * inv_mass_a / sum
B.position += correction * inv_mass_b / sum
```

当前默认使用 `slop=0.001f`、`correction_percent=0.8f`，均来自 `PhysicsWorldConfig`。若 sum=0，不修正。

### 法向与切向顺序冲量

```text
relative_velocity = velocity_b - velocity_a
closing_speed = dot(relative_velocity, normal)
```

若 closing_speed >= 0，双方正在分离，不修改。否则先计算法向冲量。双方弹性取较大值；闭合速度低于 `restitution_velocity_threshold` 时强制使用零弹性，避免静止接触微弹：

```text
impulse_magnitude = -(1 + restitution) * closing_speed
                    / (inv_mass_a + inv_mass_b)
impulse = normal * impulse_magnitude
velocity_a -= impulse * inv_mass_a
velocity_b += impulse * inv_mass_b
```

应用法向冲量后重新计算相对速度。切线固定为 `(-normal.y, normal.x)`；完全抵消切向速度所需冲量未超过 `static_friction * normal_impulse` 时使用静摩擦，否则按 `dynamic_friction * normal_impulse` 截断。两目标摩擦系数取几何平均。

Static、Tile、Collider-only 和 Kinematic 的逆质量为零，但 Kinematic 的 authored velocity 仍参与相对速度，因此移动平台能给 Dynamic 施加切向冲量，自己不会被反推。每个 `CollisionContact` 累加本步 normal/tangent impulse；Overlap 始终保持零。

### 迭代

按稳定 contact 顺序默认执行 8 次。位置修正和速度冲量分离；每次位置修正后重新构造受影响的 current shape。当前不跨帧保存冲量，因此没有 warm starting。

## 14. Overlap

Overlap 保存 manifold 并参与 Begin/Stay/End，但：

- 不修改 position；
- 不修改 velocity；
- 不改变 BodyType；
- 不自动调用伤害；
- 不应因为 penetration 深而升级成 Block。

## 15. ContactCache

每步先建立去重后的 current map，再与 previous map 比较：

```text
current - previous => Begin
current ∩ previous => Stay
previous - current => End
```

key 包含规范化完整 target pair。value 保存最近 manifold 与 response。

若同 key 在同一步出现多个 Tile/shape contact，保留规则：更早 TOI 优先；TOI epsilon 内 Block 优先于 Overlap；同 response 时 penetration 更深者优先；仍相同则保持首次稳定遍历结果。

事件顺序建议 phase 不单独分组，而按 target pair 排序后对每 pair 产生唯一 phase；这样消费方观察顺序只取决于身份，不取决于 hash 容器。

## 16. 查询

### Ray/AABB slab

对 X/Y 分别求 ray 与 min/max plane 的 t。方向轴分量近零时，origin 必须在该轴区间内，否则无命中。`t_enter=max(axis_enter)`，`t_exit=min(axis_exit)`；要求 `t_enter<=t_exit` 且区间与 `[0,max_distance]` 相交。

法线来自最大 enter t 的轴。origin 在内部时 distance=0，normal 取反 direction 的最大绝对轴。

### Ray/Circle

代入 `|origin + direction*t - center|² = radius²`。方向已单位化，选择最小非负根；origin 在内部时返回 t=0。

### Tile DDA

从 origin 所在格开始，计算下一 X/Y 网格线的 tMax 与每跨一格的 tDelta，按较小 tMax 前进。平局时两轴都前进，并按稳定顺序检查角邻格，避免从格角穿漏。遇到 filter 匹配且非 Empty 的 Tile 后做精确 cell rect ray test，再与普通 Collider 最近 hit 比较。

### all-hits、overlap 与 sweep

- Ray/Segment all-hits 遍历完整有效距离，按 distance、target 排序并对 target 去重；最近查询复用第一项；
- AABB/Circle overlap 复用离散几何，manifold normal 从 query 指向 target，结果按 target 排序；
- AABB sweep 复用 Swept AABB，只检测 AABB Collider 与 Tile；Circle 目标跳过；
- 零位移 sweep 仍走 AABB-only initial-overlap 路径，返回 target 最小者，不能复用会包含 Circle 的通用 overlap API；
- Collider 查询使用 registration 中最后一次提交的 `current_owner_origin`，不会读取尚未进入 fixed step 的外部 Transform 修改；
- Tile world-to-coordinate、DDA 和候选范围先检查 `int` 可表示性、候选计数溢出和配置上限；失败时安全跳过 Tile 部分，不执行未定义转换或无界循环；
- 查询只读当前已提交世界，不写 contact、event、stats 或 debug snapshot。

## 17. 接地、墙体和天花板派生

首版可以在 Gameplay/Body runtime 从 Block contact normal 派生：由于 normal 从 body 指向 other：

- normal.y > threshold：other 在 body 下方，body grounded；
- normal.y < -threshold：other 在 body 上方，命中天花板；
- `abs(normal.x) > threshold`：墙体。

threshold 建议配置为 `0.5f`，不要要求法线恰好等于轴向值，以便未来支持非轴形状。状态在每固定步从当前 Block contacts 重建，不能只在 Begin 时设置而忘记 End。

## 18. 调试和诊断

至少提供可开关类别：

- PhysicsCollider：current world shape；
- PhysicsPreviousCollider：previous shape；
- PhysicsBroadPhase：候选 bounds/pairs；
- PhysicsContact：contact point、normal、penetration；
- PhysicsTileCandidate：本步读取的 Tile rect；
- PhysicsContinuous：swept bounds 与 TOI；
- PhysicsQuery：ray/segment 与最近命中。

日志只用于结构错误和限频诊断，不能每个 Tile、每个 Stay 接触持续输出。

下一篇：[Tile Map 碰撞适配](06-tile-map-collision.md)
