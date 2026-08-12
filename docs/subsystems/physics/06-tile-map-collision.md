# 06｜Tile Map 碰撞适配

> **实现状态**：规则整格 Tile 候选、Block/Overlap/OneWay、负坐标、非零原点、非方格尺寸、越界策略、AABB CCD 和 Tile DDA 查询均已落地。项目仍只需实现 `ITileCollisionWorld`。

> `TileCoordinate`、`CollisionTarget`、`TileCollisionCell` 与 `ITileCollisionWorld` 已作为公共契约落地；每个 `PhysicsWorld` 可绑定一个活动 Tile World。坐标换算、候选范围、Tile 检测/求解、内部边缘抑制与查询均已实现，项目只需提供适配器。

返回：[物理文档入口](README.md)　上一篇：[碰撞算法](05-collision-algorithms.md)　下一篇：[Gameplay Runtime](07-gameplay-collision-runtime.md)

## 1. 为什么使用适配器

Tile Map 是一种非常适合空间查询的数据结构，但具体项目的 Tile 往往还包含渲染 atlas、房间生成、装饰、地形类型和脚本标记。物理核心不应 include 这些项目类型。

正确边界是：

```text
项目 TileMap / Room / Layer
       ↓ TileMapCollisionAdapter
ITileCollisionWorld（引擎契约）
       ↓ TileCollisionResolver
PhysicsWorld / CollisionSystem
```

适配器只转换数据，不执行物理步。TileCollisionResolver 只执行网格几何，不知道项目 Tile ID。

## 2. 为什么不为每格创建 Collider

假设 200×200 地图中 30% 是实心格：逐格 Collider 会产生 12,000 个持久对象、ID、注册项和粗检输入。规则网格已经能由 AABB 直接计算候选范围，没有必要把空间结构展开后再重建空间索引。

逐格 Collider 还会带来：

- 动态地图修改时大量注册/注销；
- 相邻格接缝产生冗余接触；
- ColliderId 消耗和事件身份不稳定；
- RayCast 无法利用 DDA；
- Tile 渲染数据与物理对象生命周期耦合。

首版因此把 Tile 作为结构化 `CollisionTarget`，只在发生接触或查询命中时构造轻量结果。

## 3. 目标契约

以下是当前已存在的公共契约：

```cpp
#pragma once

#include "../../core/geometry/vector2.h"
#include "../collision/collider.h"

#include <cstdint>
#include <optional>
#include <string_view>

namespace elysia::physics
{
struct TileCoordinate
{
    int x = 0;
    int y = 0;
};

enum class TileCollisionType : std::uint8_t
{
    Empty,
    Block,
    Overlap,
    OneWay
};

enum class TileOutOfBoundsPolicy : std::uint8_t
{
    Block,
    Empty
};

struct TileCollisionCell
{
    TileCollisionType type = TileCollisionType::Empty;
    CollisionFilter filter{};
    std::optional<OneWayCollision> one_way;
    PhysicsMaterial material{};
    std::string_view tag{};
};

class ITileCollisionWorld
{
public:
    virtual ~ITileCollisionWorld() = default;

    [[nodiscard]] virtual core::Vector2 world_origin() const noexcept = 0;
    [[nodiscard]] virtual core::Vector2 tile_size() const noexcept = 0;
    [[nodiscard]] virtual int columns() const noexcept = 0;
    [[nodiscard]] virtual int rows() const noexcept = 0;
    [[nodiscard]] virtual TileOutOfBoundsPolicy
        out_of_bounds_policy() const noexcept = 0;
    [[nodiscard]] virtual TileCollisionCell cell_at(
        TileCoordinate coordinate) const noexcept = 0;
};
}
```

### 契约不变量

- origin 和 tile_size 在一次 fixed step/query 中不变；
- tile_size 两轴有限且大于 epsilon；
- columns/rows 非负；
- `cell_at` noexcept，不返回项目对象引用；
- type=OneWay 时 one_way 必须有值；其他 type 的 one_way 忽略；
- type=Empty 时 filter/tag 不参与检测；
- adapter 的生命周期覆盖 PhysicsWorld 绑定期。

## 4. 结构化 Tile 身份

当前 `CollisionTarget` 使用 tagged union 语义：

```cpp
struct CollisionTarget
{
    CollisionTargetKind kind = CollisionTargetKind::Invalid;
    ColliderId collider = InvalidColliderId;
    TileCoordinate tile{};

    static CollisionTarget from_collider(ColliderId id) noexcept;
    static CollisionTarget from_tile(TileCoordinate coordinate) noexcept;
    bool is_valid() const noexcept;
    bool operator==(const CollisionTarget&) const noexcept;
    std::strong_ordering operator<=>(const CollisionTarget&) const noexcept;
};
```

首版每个 PhysicsWorld 只有一个活动 Tile World，因此 `(kind=Tile, x, y)` 在世界内唯一。未来支持多个地图源时再加入 `TileWorldId`，不要提前塞项目 room 指针。

排序规则：Collider target 排在 Tile target 前；Collider 按 ID；Tile 按 y、x。普通 Collider/Tile contact 固定让 Collider 在 pair.first，便于法线和 Gameplay Body 事件一致。

## 5. 项目适配器示例

下面示例只展示边界，具体 Tile 类型由项目替换：

```cpp
class TileMapCollisionAdapter final
    : public elysia::physics::ITileCollisionWorld
{
public:
    void bind(const ProjectTileMap* map,
              elysia::core::Vector2 origin,
              elysia::core::Vector2 tile_size) noexcept
    {
        _map = map;
        _origin = origin;
        _tile_size = tile_size;
    }

    void unbind() noexcept
    {
        _map = nullptr;
    }

    elysia::core::Vector2 world_origin() const noexcept override
    {
        return _origin;
    }

    elysia::core::Vector2 tile_size() const noexcept override
    {
        return _tile_size;
    }

    int columns() const noexcept override
    {
        return _map ? _map->width() : 0;
    }

    int rows() const noexcept override
    {
        return _map ? _map->height() : 0;
    }

    elysia::physics::TileOutOfBoundsPolicy
    out_of_bounds_policy() const noexcept override
    {
        return elysia::physics::TileOutOfBoundsPolicy::Block;
    }

    elysia::physics::TileCollisionCell cell_at(
        elysia::physics::TileCoordinate coordinate) const noexcept override
    {
        if (!_map)
            return {};

        if (coordinate.x < 0 || coordinate.x >= _map->width()
            || coordinate.y < 0 || coordinate.y >= _map->height())
        {
            return out_of_bounds_policy()
                == elysia::physics::TileOutOfBoundsPolicy::Block
                ? make_world_boundary_cell()
                : elysia::physics::TileCollisionCell{};
        }

        return convert_tile(_map->get(coordinate.x, coordinate.y));
    }

private:
    const ProjectTileMap* _map = nullptr;
    elysia::core::Vector2 _origin{};
    elysia::core::Vector2 _tile_size{};
};
```

适配器可以合并多个项目碰撞层：按项目规定的优先级读取墙体层、平台层、触发层，最后返回一个 cell。该优先级属于项目；首版物理接口每坐标只消费一个合并结果。

## 6. 绑定生命周期

推荐 Scene 顺序：

```text
on_enter:
  载入/生成 TileMap
  adapter.bind(map, origin, tile_size)
  physics_world.set_tile_world(adapter)

on_exit:
  physics_world.clear_tile_world(adapter)
  adapter.unbind()
  再销毁 TileMap
```

如果先销毁 TileMap，PhysicsWorld 中的 adapter 借用仍可能在 query 或固定步中被调用，产生悬空访问。`set_tile_world`/`clear_tile_world` 必须做实例身份检查。

动态替换地图时先 clear 旧 adapter，再 bind 新 adapter；不能在一个 fixed step 中改变 origin、size、rows 或 columns。

## 7. world-to-tile 转换

```text
tile_x = floor((world_x - origin_x) / tile_width)
tile_y = floor((world_y - origin_y) / tile_height)
```

必须使用 floor：C++ 把负浮点转 int 是向零截断，例如 `-0.2f` 会变成 0，而正确 Tile 坐标应为 -1。

例：origin=(100,50)、size=(16,32)：

| 世界点 | Tile |
| --- | --- |
| (100,50) | (0,0) |
| (115.999,81.999) | (0,0) |
| (116,82) | (1,1) |
| (99.9,50) | (-1,0) |

转换 helper 应在 tile_size 无效、输入非有限或 floor 结果超出 `int` 可表示范围时返回 nullopt，而不是除零或执行未定义浮点到整数转换。

## 8. Tile rect

```text
left = origin.x + tile.x * tile_size.x
top  = origin.y + tile.y * tile_size.y
rect = Rect(left, top, tile_size.x, tile_size.y)
```

允许构造越界坐标的 rect，因为 Block 越界策略需要把地图外覆盖区域视作虚拟格子。整数乘浮点前要显式转换，极端坐标导致非有限结果时跳过并诊断。

## 9. checked Tile 候选范围

对 candidate/swept rect：

```text
min_x = floor((left   - origin.x) / tile_width)
max_x = floor((right  - origin.x - epsilon) / tile_width)
min_y = floor((top    - origin.y) / tile_height)
max_y = floor((bottom - origin.y - epsilon) / tile_height)
```

这是模拟碰撞使用的半开模式；减 epsilon 的目的，是在 AABB 的 right 恰好等于下一格 left 时不把下一格算作面积相交候选。Overlap 和 sweep 查询改用闭合模式，左右上下边界相切的 Tile 都必须进入精确检测，`fraction == 1` 的 sweep 也必须返回。

- Empty 越界策略：range 裁剪至 `[0, columns-1] × [0, rows-1]`；
- Block 越界策略：不裁剪到地图，但 range 只来自有限 swept rect；
- candidate rect 为空：返回空 range；
- max < min：返回空 range；
- min/max floor 结果超出 `int`、候选宽高或乘积溢出、单次候选格数超过 `PhysicsWorldConfig::max_tile_candidates_per_operation` 时，整段 Tile 操作安全拒绝；不截断为不完整结果；
- 模拟拒绝计入 `PhysicsStepStats::rejected_tile_candidate_ranges`；query/ray 的 Tile 部分安全返回空，但普通 Collider 结果仍可返回。

## 10. Cell 到响应的映射

| Cell type | 几何形状 | Response | One-way |
| --- | --- | --- | --- |
| Empty | 无 | Ignore | 无 |
| Block | 整格 AABB | Block | 无 |
| Overlap | 整格 AABB | Overlap | 无 |
| OneWay | 首版整格 AABB | Block，经规则可降级 Ignore | 必须有配置 |

Filter 先于几何窄检。Cell 的 filter 与移动 Collider filter 使用和普通 Collider 相同的双向规则。

## 11. 分轴移动与统一求解

参考项目使用“先 X 后 Y，候选 Tile 限制允许位移”的方式，优点是简单、适合纯 AABB 角色。ElysiaEngine 的目标架构还需要 Circle、动态对象、普通 Collider 和事件，因此 Tile 最终应输出统一 contact，进入 CollisionSystem solver。

实现顺序可以分两步：

1. 阶段 8 先用 AABB 分轴 resolver 完成 Tile Block，建立坐标和边界测试；
2. 阶段 9 把 Tile hit 转为 `CollisionContact`，与 Swept AABB、one-way、事件缓存统一。

最终版本不能让“Tile 分轴移动”和“普通 Collider solver”各自修改同一 Dynamic 的 position 而没有固定顺序。推荐先求最早 continuous hit，再对最终 contact 集做稳定迭代。

## 12. 高速移动

仅检查 current AABB 会穿过薄墙。首版 AABB/Tile 连续路径：

1. 合并 previous/current AABB 得 swept bounds；
2. 由 swept bounds 得候选 Tile range；
3. 对每个非 Empty cell 做 swept AABB；
4. 选择最早 TOI；
5. 平局按 Tile y、x；
6. 移动到首次接触位置；
7. 去除法向速度；
8. 用剩余时间处理切向运动，并限制最大碰撞迭代次数。

不能只采用固定 128 次子步作为防穿透保证：极端速度或大 delta 仍可能超过上限。子步可作为保守辅助，真正 Continuous 模式必须基于 TOI。

## 13. 相邻 Tile 接缝

角色沿连续墙面移动时，不应被内部接缝法线卡住。当前实现的规则是：

- 只检查轴对齐法线，并在 Tile 朝向 collider 的一侧查询相邻格；
- 当前格和相邻格都是 Block，或都是相同 pass-through 方向且本次都分类为 Block 的 OneWay 时，丢弃被覆盖的内部面；
- Block/Overlap、不同 OneWay 规则、真实外轮廓和不匹配 filter 不合并；
- 过滤在初次离散/CCD 候选及 CCD 剩余时间迭代中使用同一入口，早于求解、缓存和事件；
- 外露顶面不合并，因此跨格支撑仍保留具体 TileCoordinate；
- 不要对 Tile rect 人为缩小，否则高速对象可能从缝隙穿过；
- 不要把所有相邻实心格预合并成永久 Collider，除非另做 chunk mesh 缓存和脏区更新设计。

## 14. OneWay Tile

OneWay cell 仍使用整格 AABB 做候选，但只阻挡指定表面。以常见“从下方可穿过、从上方落下会站住”的平台为例：

- adapter 返回 type=OneWay、pass_through=Up；
- actor previous bottom 应位于平台 top 的阻挡侧或 tolerance 内；
- actor 相对平台向下移动时 Block；
- actor 从平台下方向上移动时 Ignore；
- actor 已经深入平台时结合 previous side 决定，不应只凭 current overlap 推到错误一侧。

四方向规则详见[碰撞算法](05-collision-algorithms.md#12-单向平台判断)。

## 15. Drop-through 与 Tile target

当前 `DropThroughRequest` 使用结构化 target，可指向 Collider 或 Tile：

```cpp
struct DropThroughRequest
{
    ColliderId actor = InvalidColliderId;
    CollisionTarget target{};
};
```

对于 Tile platform，ignore key 是 `(actor ColliderId, TileCoordinate)`。如果角色站在由多个相邻 OneWay Tile 构成的平台上，只忽略当前支撑接触可能在下一格立即重新 Block。runtime 应从当前 grounded contacts 收集同一支撑面的相邻 Tile targets，或定义请求可携带一组当前支撑 targets。首版推荐 runtime 在请求时抓取 actor 当前所有向下支撑 OneWay Tile，并原子加入 ignore set。

恢复条件是 actor 与对应 Tile rect 完全分离并越过 tolerance；不能只靠固定毫秒计时。

## 16. Overlap Tile

Overlap Tile 可用于出口、危险区域或脚步材质触发，但物理核心只产生 Begin/Stay/End。项目根据 TileCoordinate 查询额外 Tile 元数据。

适配器返回的 `string_view tag` 只用于调试；事件不应长期保存可能指向项目临时字符串的 view。业务需要稳定语义时，通过 TileCoordinate 回查当前 map，或在项目层维护稳定的区域 ID。

## 17. Tile 查询

### RayCast

使用 2D DDA 沿网格前进，而不是构造整段覆盖的巨大二维候选框。每进入一格：

1. `cell_at`；
2. Empty/filter 不匹配则继续；
3. 对 cell rect 做精确 ray/AABB；
4. 若命中距离小于下一网格边界或当前普通 Collider 最近距离，可以返回/参与比较。

Block 与 Overlap 都可查询；Ignore/Empty 不命中。

### SegmentCast

转成有限长度 ray，DDA 在累计距离超过 segment length 时结束。

### 起点越界

Block policy 下，起点位于地图外等价于位于虚拟 Block cell 内，返回 fraction=0。Empty policy 下继续沿 ray，直到进入地图或到达最大距离。

## 18. 动态地图修改

首版 adapter 可以直接反映项目 map 的最新 cell，但修改必须发生在 fixed step 之外。建议项目提供版本号：

- fixed step 开始读取一次 version；
- 本步结束前 version 必须不变；
- version 变化后 PhysicsWorld 使相关 Tile contact cache 失效并在下一步重建；
- 从 Block 改 Empty 应产生 End；从 Empty 改 Block 若当前对象已占据该格，应生成初始穿透 contact 并稳定推出。

版本接口可在动态地图成为真实需求时加入；首版若地图静态，应在文档和 adapter 注释中明确“绑定期间不可修改”。

## 19. 与 Project-Hail-Mary 的对比

参考项目的优点：

- `TileCollisionWorld` 把项目 TileMap 与物理核心隔开；
- origin、tile size、rows/columns 足以支持规则网格；
- 只查询候选 Tile，不展开全部 Collider；
- 分轴解析容易理解并适合首个 AABB 版本。

不能直接照搬的限制：

- `bool is_tile_collidable` 无法表达 Overlap、OneWay、Filter；
- 越界行为隐藏在 PhysicsManager helper 中，接口契约不明确；
- 只有一个非 owning world 指针但没有身份校验式 clear；
- Tile 不进入统一 contact/query 身份；
- 分步上限不能严格防高速穿透；
- 只通知合成碰撞方向，没有 Begin/Stay/End 或具体 Tile 坐标；
- 算法与 body 注册、移动和 debug draw 集中在一个 Manager 中，不利于测试替换。

ElysiaEngine 应保留它的适配器边界和候选格查询思路，但把 cell 语义、目标身份、事件和 CCD 接入统一 PhysicsWorld。

## 20. Tile 专项验收

- origin=(0,0) 与非零 origin 的转换均正确；
- world 负坐标使用 floor；
- 16×32 非正方形 Tile 正确；
- AABB right/bottom 恰好在格线时不多取下一格；
- Empty/Block 越界策略分别工作；
- Block、Overlap、四方向 OneWay 均有 Begin/Stay/End；
- 高速 AABB 不穿过一格厚墙；
- 沿连续墙面滑动不被内部接缝卡住；
- Ray/Segment 返回最近 Tile 或普通 Collider；
- Tile Map 清除/替换后无悬空引用和陈旧 contact；
- adapter 不被 `engine/physics` include，依赖方向正确。

下一篇：[Gameplay 碰撞 Runtime](07-gameplay-collision-runtime.md)
