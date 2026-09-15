# 物理系统：功能与接口总览

本项目的物理系统以 `elysia::physics::PhysicsWorld` 为唯一的场景内物理世界，底层使用 Box2D 3.1.1。游戏代码只依赖 Elysia 的定义、句柄、查询和事件类型，不直接保存 Box2D 对象或包含 Box2D 头文件。

`elysia::scene::Scene` 自带一个 `PhysicsWorld`：对象加入场景时，如果同时实现 `PhysicsParticipant`，场景会自动注册它；每帧 `Scene::on_update(double)` 在普通对象更新后调用 `PhysicsWorld::advance(delta)`；对象销毁时会自动注销。因此，通常无需自行管理对象注册的时机。

## 能力清单

| 功能 | 用途 | 主要接口 |
| --- | --- | --- |
| 刚体模拟 | 静态、运动学、动态刚体；重力、阻尼、睡眠、旋转、质量 | `BodyDefinition`、`PhysicsWorld::set_velocity`、`apply_force`、`apply_impulse` |
| 碰撞体 | AABB、圆形；材质、层/掩码过滤、实体、阻挡或重叠 | `Collider`、`AabbShape`、`CircleShape`、`CollisionFilter` |
| 连续碰撞 | 对高速动态刚体启用 CCD | `BodyDefinition::bullet`、`Collider::detection_mode` |
| 固定步进与渲染插值 | 固定模拟频率，渲染读取平滑姿态 | `PhysicsWorldConfig`、`advance`、`render_pose` |
| 碰撞事件与接地状态 | Begin/Stay/End、接触流形、冲量、地面/墙/天花状态 | `ICollisionListener`、`CollisionEvent`、`contact_state`、`collect_contacts` |
| 空间查询 | 射线、线段、AABB/圆重叠、AABB 扫掠 | `raycast`、`segment_cast`、`overlap_aabb`、`overlap_circle`、`sweep_aabb` |
| TileMap 碰撞 | 方块、触发、单向平台、边界墙、局部脏区更新 | `ITileCollisionWorld`、`set_tile_world`、`update_tiles`、`request_pass_through` |
| 关节 | 距离约束/弹簧、转轴、角度限制与电机 | `create_distance_joint`、`create_revolute_joint` |
| 游戏碰撞语义 | Body/PushBox/HurtBox/HitBox/Sensor，阵营过滤及一次攻击去重 | `GameplayCollisionRuntime`、`GameplayCollisionService`、`GameplayCollisionListener` |
| 调试与统计 | 碰撞体、原生 AABB、接触、速度、关节与步进统计 | `set_debug_capture`、`debug_snapshot`、`last_step_stats` |

## 1. 物理对象与刚体控制

实现 `elysia::physics::PhysicsParticipant` 的 `GameObject` 会在 `Scene::add_object` 时自动注册。实现 `PhysicsStepParticipant` 后，`fixed_update(double fixed_delta_seconds)` 会在每一个实际物理步执行，适合读取已锁存的输入并施加力或设置速度；不要在渲染帧里按帧率累积物理控制。

```cpp
class Player final : public elysia::core::GameObject,
                     public elysia::physics::PhysicsParticipant,
                     public elysia::physics::PhysicsStepParticipant
{
public:
    elysia::physics::BodyDefinition body_definition() const override
    {
        return {
            .type = elysia::physics::BodyType::Dynamic,
            .mass_policy = elysia::physics::MassPolicy::ExplicitMass,
            .mass = 70.0f,
            .gravity_scale = 1.0f,
            .fixed_rotation = true,
            .bullet = false,
        };
    }

    std::span<const elysia::physics::Collider> collider_definitions() const override
    {
        return _colliders;
    }

    void fixed_update(double) override
    {
        set_velocity_x(_move_direction * 280.0f);
        if (_jump_requested && physics_world())
            physics_world()->apply_impulse(physics_handle(), {0.0f, -22000.0f});
    }

private:
    std::array<elysia::physics::Collider, 1> _colliders{ /* ... */ };
    float _move_direction = 0.0f;
    bool _jump_requested = false;
};
```

`PhysicsParticipant` 已提供常用快捷接口：`physics_handle()`、`physics_collider(index)`、`physics_state()`、`velocity()`、`set_velocity(...)`、`set_velocity_x(...)`、`set_velocity_y(...)` 和 `update_physics_collider(index, definition)`。

需要直接操作世界时，使用下列 `PhysicsWorld` 接口。所有对象、碰撞体和关节 ID 都是不透明句柄；传入失效句柄的命令返回 `false`，状态读取返回 `std::nullopt`。

| 目标 | 接口 |
| --- | --- |
| 注册/查询/注销 | `register_object(owner, body, colliders)`、`unregister_object(handle)`、`object_handle(owner)`、`collider_id(handle, index)` |
| 读模拟状态 | `body_state(handle)`；读渲染状态用 `render_pose(handle)` |
| 线速度与角速度 | `set_velocity`、`set_velocity_x`、`set_velocity_y`、`set_angular_velocity` |
| 重力、启用、唤醒 | `set_gravity_scale`、`set_body_enabled`、`set_awake` |
| 力与冲量 | `apply_force`、`apply_impulse`、`apply_torque`、`apply_angular_impulse`；可选 `world_point` 是世界坐标施力点 |
| 瞬移 | `teleport_object(handle, position, mode)` 或 `set_transform(handle, pose, mode)`；`TeleportVelocityMode::Clear` 会清空速度 |

物理单位使用 EU（engine units）：位置/长度为 EU，速度为 EU/s，重力为 EU/s²，角度为弧度，质量为 kg。默认 `PhysicsWorldConfig::units_per_meter` 为 100，默认固定步长为 `1.0 / 60.0` 秒。`render_pose` 是前后两次模拟姿态插值后的展示值；判定、存档和游戏逻辑应读取 `body_state`，不要把普通 `GameObject` 的位置写入当作物理瞬移。

## 2. 碰撞体、过滤与响应

`Collider` 支持两种形状：`AabbShape{ Rect local_rect }` 和 `CircleShape{ Vector2 local_center, float radius }`。一个刚体可带多个 Collider；调用 `collider_id(handle, index)` 或 `PhysicsParticipant::physics_collider(index)` 获取其稳定 ID。

```cpp
elysia::physics::Collider body_collider{
    .shape = elysia::physics::AabbShape{{-16.0f, -28.0f, 32.0f, 56.0f}},
    .filter = {.category = PlayerBits, .mask = WorldBits | EnemyBits},
    .response = elysia::physics::CollisionResponse::Block,
    .material = {.friction = 0.4f, .restitution = 0.0f},
    .tag = "player-body",
};
```

- `CollisionResponse::Block` 会产生实体接触和解算；`Overlap` 产生事件但不阻挡；`Ignore` 不参与。
- `CollisionFilter` 用 `category` / `mask` 双向匹配。相同且非零的 `group` 优先：正数强制碰撞，负数强制忽略。
- `Collider::enabled` 或 `set_collider_enabled(id, false)` 禁用形状；`update_collider(id, definition)` 替换整份已复制的定义。
- `PhysicsMaterial` 提供 `friction`、`restitution`。默认密度是 `SurfaceDensity`（kg/m²）；传感器默认不计入质量，除非设定 `sensor_contributes_mass = true`。
- `BodyDefinition::bullet = true` 与 `Collider::detection_mode = Continuous` 用于高速实体刚体。它们不会把 `Overlap` 传感器或游戏 HitBox 自动变为连续命中；高速攻击请使用第 4 节的显式扫掠查询。

## 3. 碰撞事件、接地和单向平台

实现 `ICollisionListener::on_collision_event(const CollisionEvent&)` 并用 `PhysicsWorld::add_listener(listener)` 注册。事件包含 `CollisionEventPhase::{Begin, Stay, End}`、排序稳定的 `CollisionPair`、法线/穿透/最多两个接触点的 `CollisionManifold`，以及法向和切向冲量。

```cpp
void Character::fixed_update(double)
{
    const auto state = physics_world()->contact_state(physics_handle());
    if (state.grounded)
        /* 可以起跳 */;
}
```

`contact_state(PhysicsObjectHandle)` 会合并对象全部 Collider 的 `grounded`、`ceiling`、`wall_left`、`wall_right`；也可传入 `CollisionTarget`。`collect_contacts(target, out_contacts)` 读取该目标当前的接触快照。法线方向以 `CollisionPair::first` 指向 `second`；读取单个目标时应优先使用 `contact_state`，避免自行反转法线。

单向碰撞由 `Collider::one_way` 或 `TileCollisionCell::one_way` 配置，其中 `OneWayCollision::pass_through` 可组合 `Up`、`Down`、`Left`、`Right`。下落穿透不是修改速度：对每个当前支撑目标调用 `request_pass_through(actor_collider, target)`。该接口仅接受带单向规则的目标，会唤醒角色，并维持忽略直到两个形状分离；角色同时站在多个格子上时要逐格请求。

## 4. 无状态空间查询

查询全部经过 `CollisionFilter`，忽略禁用形状和 `CollisionResponse::Ignore`。`raycast`、`segment_cast` 与 `sweep_aabb` 返回最近命中（无命中为 `std::nullopt`）；`*_all` 把结果写入调用者提供的 `std::vector`，按距离排序并按目标去重。

| 需求 | 查询结构与接口 | 返回 |
| --- | --- | --- |
| 激光/视线 | `RayCastQuery` + `raycast` / `raycast_all` | `CollisionQueryHit` |
| 起点到终点的直线 | `SegmentCastQuery` + `segment_cast` / `segment_cast_all` | `CollisionQueryHit` |
| 范围内方形选取 | `AabbOverlapQuery` + `overlap_aabb` | `CollisionOverlapQueryHit` |
| 范围内圆形选取 | `CircleOverlapQuery` + `overlap_circle` | `CollisionOverlapQueryHit` |
| 高速子弹或角色盒子预判 | `AabbSweepQuery` + `sweep_aabb` | 最近 `CollisionQueryHit` |

`CollisionQueryHit` 给出 `target`（Collider 或 Tile）、命中 `point`、`normal`、距离 `distance`、0–1 的 `fraction` 和对方的 `response`。查询不会产生碰撞事件、伤害或物理响应；它适合把高速攻击的“命中时刻”明确交给游戏规则层处理。

## 5. TileMap 碰撞

实现 `ITileCollisionWorld`：提供地图原点、格子尺寸、行列数、越界策略和 `cell_at(TileCoordinate)`。`TileCollisionCell::type` 可为 `Empty`、`Block`、`Overlap` 或 `OneWay`，同时可携带独立的过滤器、材质、tag 与单向规则。

```cpp
world.set_tile_world(my_tile_collision_world);

// 修改格子后，仅重建包含变动格的闭区间。
world.update_tiles({dirty_left, dirty_top}, {dirty_right, dirty_bottom});

// 场景结束或切换地图时，传入同一个地图实例解除绑定。
world.clear_tile_world(my_tile_collision_world);
```

TileMap 是借用引用，`ITileCollisionWorld` 实例必须比绑定持续更久。`TileOutOfBoundsPolicy::Block` 会生成外围四面边界墙，`Empty` 则不生成。相邻、过滤规则相同的实体格会抑制内部共享面；暴露面仍可碰撞。当前不包含斜坡、半格、分块流式加载或合并轮廓。

## 6. 关节

`create_distance_joint(const DistanceJointDefinition&)` 创建两刚体之间的固定距离或弹簧；`create_revolute_joint(const RevoluteJointDefinition&)` 创建转轴，可选角度限制和电机。创建成功返回 `JointHandle`；使用 `joint_state(handle)` 读取两端锚点、反作用力和扭矩，使用 `destroy_joint(handle)` 删除。

两个定义都用局部锚点，且 `collide_connected` 默认 `false`。距离关节可设置 `spring`、`frequency_hz`、`damping_ratio`；转轴关节可设置 `enable_limit`、`lower_angle`、`upper_angle`、`enable_motor`、`motor_speed`、`max_motor_torque`。

## 7. 面向游戏的碰撞运行时

物理层只报告形状接触；`elysia::gameplay::collision::GameplayCollisionRuntime` 把 Collider 绑定为角色、阵营和角色用途，并路由更适合玩法的事件。`GameplayScene` 自己持有该运行时；场景激活时由 `GameplayCollisionService` 挂接，因此一般玩法代码通过 `GameplayCollisionService::instance()` 调用，而不是另外创建运行时。

| ColliderRole | 绑定方式 | 事件 |
| --- | --- | --- |
| `Body` | `ActorCollisionRig::body` 或 `bind_collider` | `GameplayCollisionListener::on_body_contact` |
| `PushBox` | `ActorCollisionRig::push_box` 或 `bind_collider` | `on_push_box_overlap`（仅两个 PushBox） |
| `HurtBox` | `ActorCollisionRig::hurt_boxes` 或 `bind_collider` | 被 HitBox 命中时作为目标 |
| `HitBox` | `bind_hit_box(HitBoxBinding)` | `on_hit_overlap` |
| `Sensor` | `ActorCollisionRig::sensors` 或 `bind_collider` | `on_sensor_overlap`（仅与 Body 的 Overlap） |

```cpp
auto* collision = elysia::gameplay::collision::GameplayCollisionService::instance();

collision->bind_actor({
    .owner = player_id,
    .team = elysia::gameplay::collision::teams::Player,
    .body = player_body_id,
    .hurt_boxes = {player_hurtbox_id},
});

collision->bind_hit_box({
    .collider = {attack_collider_id, player_id,
                 elysia::gameplay::collision::teams::Player,
                 elysia::gameplay::collision::ColliderRole::HitBox},
    .instigator = player_id,
    .attack_instance = attack_instance_id,
    .attack_definition = slash_definition_id,
});
```

`HitBox` 只在 Begin 时对 `HurtBox` 路由，且仅阵营关系为 `Hostile` 时触发；同一个 `(attack_instance, hurt_actor)` 在攻击存续期内只会命中一次。攻击结束后必须调用 `end_attack_instance(id)` 清除去重记录和该攻击的 HitBox 绑定；角色或形状生命周期结束时调用 `unbind_actor(actor)` 或 `unbind_collider(collider)`。默认规则是同阵营 Friendly、任一 Neutral 或无效阵营 Neutral、其他阵营 Hostile；可由 `GameplayCollisionRuntime::set_team_relation_resolver(...)` 替换。

`GameplayCollisionListener` 可覆写 `on_body_contact`、`on_push_box_overlap`、`on_hit_overlap` 和 `on_sensor_overlap`。监听器在事件回调中可以请求解绑等变更，当前事件批次保持稳定；监听对象本身必须在这批回调结束前仍然存活。

## 8. 调试、统计和生命周期注意事项

调试时用 `set_debug_capture(PhysicsDebugCapture::Shapes | PhysicsDebugCapture::Contacts)` 选择捕获项，再用 `debug_snapshot()` 读取。`PhysicsDebugCapture` 支持 `Shapes`、`BroadPhase`、`Contacts`、`Velocities`、`Joints` 和 `All`；`submit_physics_debug_snapshot(snapshot, debug_draw)` 可提交至引擎调试绘制。`PhysicsDebugShape` 同时保存前一/当前姿态和 `native_bounds`：后者是物理步的轴对齐原生包围盒，不是旋转 Collider 轮廓或 CCD 轨迹。

`last_step_stats()` 提供已注册对象/Collider、醒着的刚体、关节数、接触数、步进耗时和丢弃固定步数；`accumulator_seconds()` 可查看未消费的帧时间；`reset()` 清空整个世界。

最后的实践边界：一个 `Scene` 只应使用它自身的 `PhysicsWorld`；不要缓存原生 Box2D ID；地图绑定和监听器都要先于世界销毁；普通位置直接改写后必须用 `teleport_object` 或 `set_transform` 同步物理；渲染读 `render_pose`，判定读 `body_state`。这样可以避免渲染、碰撞和游戏规则各自使用不同的位置来源。
