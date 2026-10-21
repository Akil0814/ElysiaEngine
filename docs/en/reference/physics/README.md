# Physics System: Features and Interfaces

`elysia::physics::PhysicsWorld` is the sole per-scene physics world, backed by Box2D 3.1.1. Games depend only on Elysia definitions, handles, queries, and events, never retaining Box2D objects or including its headers.

`elysia::scene::Scene` optionally creates `FixedStepRuntime` and `PhysicsWorld`; physics requires fixed stepping. `PhysicsParticipant` objects automatically register on addition and unregister before reclamation. Internal updates advance fixed steps after ordinary objects/UI presentation animation: scene fixed hook, then `PhysicsWorld::step`, followed by `finalize_frame` for interpolation at frame end.

Games extend `on_before_update`/`on_after_update` without overriding or manually driving base scheduling.

This reference describes features, APIs, and limitations for developers querying public capabilities. See [physics architecture](../../architecture/subsystems/physics/02-target-architecture.md) for execution/ownership.

## Feature List

| Feature | Purpose | Main interfaces |
| --- | --- | --- |
| Rigid bodies | Static/kinematic/dynamic; gravity, damping, sleep, rotation, mass | `BodyDefinition`, `PhysicsWorld::set_velocity`, `apply_force`, `apply_impulse` |
| Colliders | AABB/circle, materials, layers/masks, solid/block/overlap | `Collider`, `AabbShape`, `CircleShape`, `CollisionFilter` |
| Continuous collision | CCD for fast dynamic bodies | `BodyDefinition::bullet`, `Collider::detection_mode` |
| Fixed scheduling | `Scene` accumulation/catch-up limits | `FixedStepConfig`, scene-owned `FixedStepRuntime` |
| Simulation/frame interpolation | Simulate fixed steps, finalize interpolation | `PhysicsWorld::step`, `finalize_frame` |
| Render snapshots | Interpolate previous/current poses | `PhysicsWorld::render_pose` |
| Events/contact state | Begin/Stay/End, manifolds, impulses, ground/wall/`ceiling` | `ICollisionListener`, `CollisionEvent`, `contact_state`, `collect_contacts` |
| Spatial queries | Rays, segments, AABB/circle overlaps, AABB sweeps | `raycast`, `segment_cast`, `overlap_aabb`, `overlap_circle`, `sweep_aabb` |
| TileMap | Blocks, triggers, one-way platforms, boundaries, dirty regions | `ITileCollisionWorld`, `set_tile_world`, `update_tiles`, `request_pass_through` |
| `Joints` | Distance/springs, hinges, limits, motors | `create_distance_joint`, `create_revolute_joint` |
| Gameplay semantics | `Body`/`PushBox`/`HurtBox`/`HitBox`/`Sensor`, teams, attack deduplication | `GameplayCollisionRuntime`, `GameplayCollisionService`, `GameplayCollisionListener` |
| Diagnostics/statistics | `Shapes`, native AABBs, contacts, velocities, joints, steps | `set_debug_capture`, `debug_snapshot`, `last_step_stats` |

## 1. Physics Objects and Body Control

GameObjects implementing `PhysicsParticipant` automatically register in `Scene::add_object`. `PhysicsStepParticipant`::`fixed_update(double fixed_delta_seconds)` runs every actual physics step for latched input, forces, and velocity. Do not accumulate control at rendering rate.

See [physics guide](../../user-guide/systems/physics.md#minimal-physics-object) for complete objects, colliders, and steps.

`PhysicsParticipant` provides physics_handle, `physics_collider(index)`, physics_state, velocity, `set_velocity`, `set_velocity_x`/y, `update_physics_collider(index, definition)`.

Direct world APIs follow. Object/collider/joint IDs are opaque handles; invalid commands return `false`, reads nullopt.

| Goal | Interface |
| --- | --- |
| Register/query/unregister | `register_object(owner, body, colliders)`, `unregister_object(handle)`, `object_handle(owner)`, `collider_id(handle, index)` |
| Simulation/presentation state | `body_state(handle)` / `render_pose(handle)` |
| Linear/angular velocity | `set_velocity`, `set_velocity_x`, `set_velocity_y`, `set_angular_velocity` |
| Gravity/enablement/wake | `set_gravity_scale`, `set_body_enabled`, `set_awake` |
| Force/impulse | `apply_force`, `apply_impulse`, `apply_torque`, `apply_angular_impulse`; optional `world_point` uses world coordinates |
| Teleport | `teleport_object(handle, position, mode)` / `set_transform(handle, pose, mode)`; `TeleportVelocityMode::Clear` clears velocity |

Units: position/length EU, velocity EU/s, gravity EU/s², radians, kg. `PhysicsWorldConfig::units_per_meter` defaults 100; fixed step 1.0/60.0 seconds. `render_pose` is interpolated presentation; decisions/saves/logic read `body_state`. `GameObject` position writes are not teleports.

## 2. Colliders, Filtering, and Response

`Collider` supports `AabbShape{ Rect local_rect }` and `CircleShape{ Vector2 local_center, float radius }`. Multiple colliders per body have stable IDs via collider_id(handle,index) / `PhysicsParticipant::physics_collider(index)`.

See [filtering/response](../../user-guide/systems/physics.md#colliders-filtering-and-response) for examples.

- `Block` creates solid contacts/solving; `Overlap` events without obstruction; `Ignore` none.
- `category`/`mask` match both directions. Equal nonzero `group` takes priority: positive forces, negative ignores.
- `Collider::enabled` / set_collider_enabled disables; update_collider replaces copied definitions.
- `PhysicsMaterial` supplies `friction`/`restitution`. `SurfaceDensity` uses kg/m²; sensors contribute no mass unless `sensor_contributes_mass = true`.
- `BodyDefinition::bullet = true` / `Collider::detection_mode = Continuous` support fast solid bodies, not continuous `Overlap`/`HitBox` hits; use section 4 sweeps.

## 3. Events, Grounding, and One-Way Platforms

Implement `ICollisionListener::on_collision_event(const CollisionEvent&)` and add_listener(listener). Events contain Begin/Stay/End, stably ordered `CollisionPair`, `CollisionManifold` `normal`/penetration/up to two points, and `normal`/tangent impulses.

See [listener/query/cleanup examples](../../user-guide/systems/physics.md#listener-query-and-cleanup-examples).

`contact_state(PhysicsObjectHandle)` merges `grounded`/`ceiling`/`wall_left`/`wall_right` across colliders; `CollisionTarget` also works. `collect_contacts`(`target`,out_contacts) reads current snapshots. Normals `point` first→`second`; prefer `contact_state` for single targets instead of reversing manually.

`Collider::one_way` / `TileCollisionCell::one_way` configures `OneWayCollision::pass_through` combinations Up/`Down`/`Left`/`Right`. Drop-through uses `request_pass_through`(actor_collider,`target`) for every support rather than changing velocity. Only one-way targets qualify; it wakes actors/ignores until separation. Multi-cell support needs per-cell requests.

## 4. Stateless Spatial Queries

Queries apply `CollisionFilter` and ignore disabled/`Ignore` shapes. `raycast`/`segment_cast`/`sweep_aabb` return nearest optional; `*_all` fills caller vectors sorted by `distance`/deduplicated by `target`.

| Need | Structure/interface | Result |
| --- | --- | --- |
| Laser/line of sight | `RayCastQuery` + `raycast` / `raycast_all` | `CollisionQueryHit` |
| Start/end line | `SegmentCastQuery` + `segment_cast` / `segment_cast_all` | `CollisionQueryHit` |
| Rectangular selection | `AabbOverlapQuery` + `overlap_aabb` | `CollisionOverlapQueryHit` |
| Circular selection | `CircleOverlapQuery` + `overlap_circle` | `CollisionOverlapQueryHit` |
| Fast bullets/actor boxes | `AabbSweepQuery` + `sweep_aabb` | Nearest `CollisionQueryHit` |

`CollisionQueryHit` provides `target` (`Collider`/Tile), `point`, `normal`, `distance`, 0–1 `fraction`, `response`. Queries create no events/damage/`response`; games decide fast-attack hit timing.

## 5. TileMap Collision

Implement `ITileCollisionWorld` with origin, cell size, dimensions, bounds policy, `cell_at(TileCoordinate)`. `TileCollisionCell::type` supports `Empty`/`Block`/`Overlap`/`OneWay` with filters/material/tag/rules.

See [tile map guide](../../user-guide/systems/tilemap-collision.md) for bind/refresh/detach.

Maps are borrowed and outlive binding. `Block` creates four perimeter walls; `Empty` none. Matching adjacent solid cells suppress shared faces while exposed faces collide. No slopes, partial cells, streaming, or merged contours.

## 6. Joints

`create_distance_joint` creates fixed-`distance`/`spring` constraints; `create_revolute_joint` hinges with optional limits/motors. Creation returns `JointHandle`; joint_state reads anchors/reaction force/torque; destroy_joint removes.

Definitions use local anchors and default `collide_connected`=`false`. Distance settings: `spring`/`frequency_hz`/`damping_ratio`. Revolute: `enable_limit`/`lower_angle`/`upper_angle`/`enable_motor`/`motor_speed`/`max_motor_torque`.

## 7. Gameplay Collision Runtime

Physics reports shape contacts only; `GameplayCollisionRuntime` binds actors/teams/roles and routes gameplay events. `GameplayScene` owns it; `GameplayCollisionService` attaches on activation. Ordinary code uses `GameplayCollisionService::instance()` rather than creating another runtime.

| `ColliderRole` | Binding | Event |
| --- | --- | --- |
| `Body` | `ActorCollisionRig::body` / `bind_collider` | `GameplayCollisionListener::on_body_contact` |
| `PushBox` | `ActorCollisionRig::push_box` / `bind_collider` | `on_push_box_overlap` (two PushBoxes only) |
| `HurtBox` | `ActorCollisionRig::hurt_boxes` / `bind_collider` | Target of `HitBox` hits |
| `HitBox` | `bind_hit_box(HitBoxBinding)` | `on_hit_overlap` |
| `Sensor` | `ActorCollisionRig::sensors` / `bind_collider` | `on_sensor_overlap` (`Overlap` with `Body` only) |

See [gameplay collision](../../user-guide/gameplay/collision.md) for paired bindings.

`HitBox` routes only Begin against hostile `HurtBox`; each (attack_instance,hurt_actor) hits once during the attack. End with `end_attack_instance(id)` to clear deduplication/bindings; unbind_actor/unbind_collider when their lifetimes end.

Default relation first makes equal valid teams Friendly (including Neutral), then any Neutral/invalid Neutral, otherwise distinct valid teams `Hostile`. Hit checks use registered instigator teams, not `HitBox` team alone. Replace through `GameplayCollisionRuntime::set_team_relation_resolver(...)`.

Listeners override `on_body_contact`/`on_push_box_overlap`/`on_hit_overlap`/`on_sensor_overlap`. Callback mutations such as unbinding preserve batch stability; listeners themselves must survive the batch.

## 8. Debugging, Statistics, and Lifetime

Select capture with `set_debug_capture`(`Shapes` | `Contacts`), read `debug_snapshot()`.

`PhysicsDebugCapture` supports `Shapes`, `BroadPhase`, `Contacts`, `Velocities`, `Joints`, `All`. submit_physics_debug_snapshot forwards to DebugDraw.

`PhysicsDebugShape` stores previous/current poses and `native_bounds`: native simulation AABBs, not rotated outlines/CCD trajectories.

`last_step_stats` reports objects/colliders/bodies/joints/contacts/step statistics. Accumulation, ticks, interpolation, dropped steps belong to separate `FixedStepRuntime`, queried via scene `fixed_step_stats()`. `PhysicsWorld`::reset clears the world; scene Reset resets fixed runtime before game on_reset without clearing every business object.

Use each scene's own world, never cached native IDs. Map/listener lifetimes must end before world destruction. After direct position writes, synchronize through `teleport_object`/`set_transform`. Render from `render_pose`; decide from `body_state`, keeping rendering/collision/rules aligned.
