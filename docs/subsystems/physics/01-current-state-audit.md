# 01｜当前状态与剩余限制审计

返回：[物理文档入口](README.md)　下一篇：[目标架构](02-target-architecture.md)

## 1. 结论

物理模块已经从“契约骨架”进入可运行的无旋转 2D 首版。默认构造的 `PhysicsWorld` 不依赖全局 Service 或启动配置；它自带 SAP、离散检测、Swept AABB 和默认响应策略。Scene 与 Gameplay 生命周期也已闭合。

## 2. 子系统状态

| 子系统 | 状态 | 当前实现 |
| --- | --- | --- |
| Body | 完成 | Static/Kinematic/Dynamic、重力、力、质量、阻尼、限速、Transform 写回 |
| 注册与 ID | 完成 | 原子验证、稳定 handle、单调 ColliderId、pending 安全边界、静默清理 |
| 世界形状 | 完成 | previous/current AABB 与 Circle、current/swept bounds |
| 宽相 | 完成 | Brute Force oracle、SAP 默认索引、AABB query、稳定排序去重 |
| 离散窄相 | 完成 | AABB/AABB、Circle/Circle、AABB/Circle，包含接触和退化情况 |
| CCD | 首版完成 | AABB/AABB 与 AABB/Tile；Circle 回退离散 |
| 响应与求解 | 完成 | Ignore/Overlap/Block、one-way、逆质量位置和法向速度修正 |
| 接触事件 | 完成 | ContactCache、Begin/Stay/End、listener 批次快照 |
| Tile | 完成 | floor 坐标换算、候选格、负坐标、非方格、越界策略 |
| Query | 完成 | AABB slab、Circle 二次方程、Tile DDA、稳定最近命中 |
| Gameplay | 完成 | Body/PushBox/Hit 路由、默认 Team、攻击实例去重、自动 Scene runtime |
| 诊断 | 完成 | 步统计、dropped steps、形状/宽相/Tile/contact/velocity 调试快照 |

## 3. 已删除的旧 API

- `PhysicsService` 与 `CollisionStrategyFactories`；
- CollisionSystem 的四个独立 setter；
- `PhysicsBodyView`、`ColliderView`；
- `PhysicsSystem::clear_forces`；
- 旧 `PhysicsSystem::step` 与 `CollisionSystem::dispatch_events`。

替代关系如下：

| 旧概念 | 当前概念 |
| --- | --- |
| 部分配置策略槽 | 完整 `CollisionStrategySet` |
| `ColliderView` 指针快照 | 值语义 `CollisionShapeView` |
| 分散 Body/Transform view | `PhysicsObjectState` |
| 本步末尾清力 | `integrate` 读取后立即清力 |
| 全局策略 Service | 默认策略函数或构造时整体注入 |

## 4. 生命周期语义

- register/unregister、listener、teleport 和 Tile 变更在 `advance`/事件回调期间进入 pending 操作，在安全边界应用；
- unregister、destroy、reset、Tile clear 和 teleport 静默移除相关缓存；
- 自然分离、Collider disable 和对象 inactive 通过本步 contact 消失产生 End；
- listener 按批次快照分发，本批次中的 add/remove 不改变其余回调；
- ColliderId 在同一 world 生命周期内不复用，reset 后计数也不倒退；
- Provider 的 Body 地址与 Collider span 地址/长度必须覆盖注册期并保持稳定。

## 5. 已知首版限制

1. Circle 没有连续检测，高速圆形可能穿过薄障碍；
2. AABB CCD 每个 Body 每步最多处理 4 次剩余时间撞击，但它仍不是支持旋转/曲面的通用刚体保守推进器；
3. solver 没有摩擦、弹性、旋转或角动量；
4. Tile 只支持规则整格 AABB，没有斜坡、半砖和自定义多边形；
5. query 是只读最近命中，不提供 all-hits 或 overlap-area 公共 API；
6. 所有物理生命周期调用假定在主线程；
7. Kinematic 为无限质量，不会被碰撞推动；
8. 一个 PhysicsWorld 只允许一个活动 Tile World。

## 6. 扩展审计

四叉树可作为新的 `IBroadPhaseIndex` 实现加入，不需要修改检测、响应、World 或 Gameplay。实现只能长期保存 ColliderId、bounds 和过滤快照，不得缓存 `Collider*` 或跨帧 view 指针。必须以 `BruteForceBroadPhaseIndex` 为 oracle 比较候选超集和稳定顺序。

新形状需要同时扩展局部 `ColliderShape`、`WorldColliderShape`、世界转换、bounds、离散策略、query 和调试适配；不能只在某一个 detector 中私自识别。

## 7. 测试基线

当前 physics 标签覆盖：数据契约、积分、过滤、形状检测、Brute/SAP oracle、注册与 ID 生命周期、固定步上限、接触事件、Block/Overlap、CCD、Tile、one-way、drop-through、Ray/Segment、Gameplay Service 与具体 Runtime。

性能验收仍采用结构指标而不是机器相关毫秒阈值：记录 proxy、pair、narrow test、Tile sample、CCD 与 solver iteration；SAP 候选不能超过 Brute Force 全组合，并且真实检测结果必须一致。
