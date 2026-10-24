# Physics System: Features and Interfaces

`elysia::physics::PhysicsWorld` is the sole per-scene physics world, backed by Box2D 3.1.1. Games depend only on Elysia definitions, handles, queries, and events, never retaining Box2D objects or including its headers.

`elysia::scene::Scene` optionally creates `FixedStepRuntime` and `PhysicsWorld`; physics requires fixed stepping. `PhysicsParticipant` objects automatically register on addition and unregister before reclamation. Internal updates advance fixed steps after ordinary objects/UI presentation animation: scene fixed hook, then `PhysicsWorld::step`, followed by `finalize_frame` for interpolation at frame end.

Games extend `on_before_update`/`on_after_update` without overriding or manually driving base scheduling.

This reference describes features, APIs, and limitations for developers querying public capabilities. See [physics architecture](../../architecture/subsystems/physics/02-target-architecture.md) for execution/ownership.

## Feature List

| Feature | Purpose | Main interfaces |
| --- | --- | --- |
| Rigid bodies | Static/kinematic/dynamic; gravity, damping, sleep, rotation, mass | `BodyDefinition`, `PhysicsWorld::set_velocity`, `apply_force`, `apply_impulse` |
| Colliders | Four area-bearing shapes and two-sided Segments, materials, filters, responses | `Collider`, `BoxShape`, `CircleShape`, `CapsuleShape`, `ConvexPolygonShape`, `SegmentShape` |
| Connected terrain | Static/Kinematic open and closed one-sided chains | `ChainDefinition`, `create_chain`, `chain_state`, `chain_collider` |
| Kinematic targets | Submit position and angle every fixed step | `set_target_transform` |
| Continuous collision | CCD for fast dynamic bodies | `BodyDefinition::bullet`, `Collider::detection_mode` |
| Fixed scheduling | `Scene` accumulation/catch-up limits | `FixedStepConfig`, scene-owned `FixedStepRuntime` |
| Simulation/frame interpolation | Simulate fixed steps, finalize interpolation | `PhysicsWorld::step`, `finalize_frame` |
| Render snapshots | Interpolate previous/current poses | `PhysicsWorld::render_pose` |
| Events/contact state | Begin/Stay/End, manifolds, impulses, ground/wall/`ceiling` | `ICollisionListener`, `CollisionEvent`, `contact_state`, `collect_contacts` |
| Spatial queries | Rays, segments, four-shape overlaps and translation casts | `raycast`, `segment_cast`, `overlap_shape`, `shape_cast`, `shape_cast_all` |
| TileMap | Blocks, triggers, one-way platforms, boundaries, dirty regions | `ITileCollisionWorld`, `set_tile_world`, `update_tiles`, `request_pass_through` |
| Joints | Eight typed constraints/filtering and runtime controls | `create_*_joint`, `update_*_joint`, `joint_state`, `destroy_joint` |
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

`Collider` supports `BoxShape`, `CircleShape`, `CapsuleShape`, `ConvexPolygonShape` and `SegmentShape`. All geometry uses body-local coordinates. Multiple colliders per body have stable IDs through `collider_id(handle, index)` or `PhysicsParticipant::physics_collider(index)`.

| Shape | Definition |
| --- | --- |
| Box | `BoxShape{local_center, half_extents, local_angle}` with positive half extents; `BoxShape::from_rect(rect)` creates an unrotated local box |
| Circle | `CircleShape{local_center, radius}` with a positive radius |
| Capsule | `CapsuleShape{local_first, local_second, radius}`; coincident endpoints behave as a circle; capsules below native length precision collapse to a midpoint circle |
| Convex polygon | `ConvexPolygonShape::local_vertices` holds at most 8 vertices; `vertex_count` is 3–8, ordered along the boundary in either winding |
| Segment | `SegmentShape{local_first, local_second}`, zero-thickness and two-sided, Static/Kinematic only |

Concave, self-intersecting, duplicate, collinear/degenerate and non-finite vertices are rejected. Concave decomposition is not automatic. Registration and queries share validation and unit conversion; geometry must also meet native precision and finite mass/inertia requirements. `update_collider` preserves `ColliderId`; invalid replacements preserve the original shape. Independent static convex polygons can form slopes; Tile cells remain rectangular.

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

All queries use `CollisionQueryFilter`: `collision` holds categories, masks and groups; `ignored_object` excludes every collider on one object; `include_sensors` defaults to `true`. Disabled and `Ignore` shapes are always excluded. Queries do not apply directional-platform movement response rules; game logic decides how to use each hit.

| Need | Interface | Result |
| --- | --- | --- |
| Ray | `RayCastQuery` + `raycast` / `raycast_all` | `CollisionQueryHit` |
| Start/end line | `SegmentCastQuery` + `segment_cast` / `segment_cast_all` | `CollisionQueryHit` |
| Any supported overlap shape | `ShapeOverlapQuery{shape, pose, filter}` + `overlap_shape` | Output `CollisionOverlapQueryHit` vector |
| Translating shape | `ShapeCastQuery{shape, pose, displacement, filter}` + `shape_cast` / `shape_cast_all` | Nearest or all `CollisionQueryHit` results |

`shape` uses the four area-bearing `ColliderShape` alternatives (Segment query sources are rejected), `pose` is a world pose, and `displacement` is a world translation. The angle stays fixed during a cast; rotational sweeps are not supported. The closest result matches the first all-hit result. All hits are sorted by `fraction`, then target ID, and deduplicated by target; separate colliders on one body remain separate hits.

Hits contain `target` (Collider or Tile), `point`, `normal`, `distance`, `fraction`, `response` and `initial_overlap`. Starting inside a target reports `initial_overlap=true`, zero distance/fraction, the query origin or pose origin as the point, and a zero normal. These sentinel values are not a contact manifold. A zero-translation shape cast only checks initial overlap. Invalid queries return no hits and clear output vectors.

Queries generate no collision events, damage or physical response. Fast Sensor/HitBox detection still requires explicit gameplay casts and result consumption; rigid-body CCD does not automatically provide continuous attack detection.

## 5. TileMap Collision

Implement `ITileCollisionWorld` with origin, cell size, dimensions, bounds policy, `cell_at(TileCoordinate)`. `TileCollisionCell::type` supports `Empty`/`Block`/`Overlap`/`OneWay` with filters/material/tag/rules.

See [tile map guide](../../user-guide/systems/tilemap-collision.md) for bind/refresh/detach.

Maps are borrowed and outlive binding. `Block` creates four perimeter walls; `Empty` none. Matching adjacent solid cells suppress shared faces while exposed faces collide. No slopes, partial cells, streaming, or merged contours.

## 6. Eight Joint Types and Runtime Settings

Creation returns `JointHandle`. Definitions fix bodies, local anchors, reference angles and local axes; finite nonzero axes are normalized. Mechanical joints require at least one Dynamic endpoint. Mouse requires Static first and Dynamic second. Filter requires two distinct valid bodies only. Each mutable settings type is fully replaced by its matching `update_*_joint(handle, settings)`, preserving the native joint and waking its bodies. Filter has no empty settings API.

| Type / creation API | Main Settings fields | `JointState.details` |
| --- | --- | --- |
| `create_distance_joint` | `length`, spring, length limits, linear motor | `DistanceJointState`: current length, speed, motor force |
| `create_revolute_joint` | `target_angle`, spring, angle limits, rotary motor | `RevoluteJointState`: angle, angular velocity, motor torque |
| `create_weld_joint` | `linear_frequency_hz`, `angular_frequency_hz`, matching `*_damping_ratio` | `WeldJointState`: `linear_error`, `angular_error` |
| `create_prismatic_joint` | `enable_spring`, `target_translation`, frequency/damping, `enable_limit`, `lower_translation`/`upper_translation`, `enable_motor`, `motor_speed`/`max_motor_force` | `PrismaticJointState`: `translation`, `speed`, `motor_force` |
| `create_wheel_joint` | Suspension `enable_spring`, frequency/damping, translation limits, rotary `motor_speed`/`max_motor_torque` | `WheelJointState`: suspension `translation`/`speed`, `angular_velocity`, `motor_torque` |
| `create_mouse_joint` | World `target`, `frequency_hz`/`damping_ratio`, `max_force` | `MouseJointState`: `drag_point`, `error`; the initial target fixes the drag anchor |
| `create_motor_joint` | Body A local `linear_offset`, relative `angular_offset`, `max_force`/`max_torque`, `correction_factor` | `MotorJointState`: local displacement and angle errors |
| `create_filter_joint` | None | `FilterJointState`: pair collision disabled |

Settings default to `collide_connected=false`. Length, translation speed, force and torque use EU units; angles use radians. Frequency, damping and maximum output must be nonnegative, limits ordered, and correction factor within `[0,1]`. Distance lengths must be positive; disabling its spring makes limits and the motor ineffective. Revolute limits lie within ±0.99π.

Prismatic defaults to no spring, limits or motor; Wheel enables suspension by default. General springs use 2 Hz and damping ratio 0.7; Weld uses zero frequencies and damping ratios 1. Mouse defaults to maximum force 1000, 4 Hz and damping ratio 1. Motor defaults to maximum force 100, maximum torque 10000 and correction factor 0.3. Other motors default off with zero speed/output.

Common `joint_state` fields include endpoint identities `first`/`second`, world anchors, `reaction_force` and `reaction_torque`. Filter imposes no mechanical constraint and reports zero reaction force/torque. Its dotted debug line marks filtering only; destroying it restores normal collision rules. Bounded constraints can retain error; measurements show the actual result.

Invalid parameters, wrong types and invalid handles return `false` or an empty `optional`, preserving old settings. Callback values enter the FIFO queue by copy. Updates can immediately follow deferred creation; execution rechecks endpoint validity and type. Pending creation, destruction, endpoint removal and Reset have no readable state. Six `joint_*` physics pages expose controls and state; Mouse supports left-button target dragging and Filter toggles pair collision.
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

## Segment and Chain Terrain

`SegmentShape{local_first, local_second}` describes a two-sided, zero-thickness local edge on Static or Kinematic objects only. It uses normal Collider filtering, material, response, enablement and stable IDs, contributes no mass, and preserves the old shape on invalid updates. Edge-edge physical solving is not promised. Overlap/cast sources remain boxes, circles, capsules and convex polygons only.

For connected terrain, call `create_chain(owner, definition)` on a registered Static/Kinematic object. `ChainDefinition` copies and owns `local_vertices` and `tag`, uses one `filter` and `material`, and always represents solid terrain detectable by sensors. An open chain needs at least two actual vertices; endpoint ghosts extrapolate the end edges unless `ghost_before`/`ghost_after` are supplied. Ghosts generate no collision edges. A closed chain (`loop=true`) needs at least four vertices without repeating the first. Edges must be at least 0.005 meters after unit conversion. Repeated, degenerate, intersecting, non-finite or immediately reversing geometry is rejected. Input order is preserved.

The front normal of local edge `a→b` is `(dy, -dx)`, then rotated with its body. In Y-down coordinates, a horizontal left-to-right edge blocks its upper side. Debug drawing shows edges and front arrows. Chain physical contacts and rays are one-sided; `overlap_shape` and `shape_cast` are geometric and can also hit the back. Gameplay consuming casts for movement must choose how to handle this difference.

`chain_state(handle)` reports owner, open/closed mode, vertex count and segment count. `chain_collider(handle, index)` returns the ColliderId of an actual edge; `chain_segment_info(id)` resolves its Chain, owner and zero-based index. Queries/events still use `CollisionTarget::Collider`. Owner contact state includes Chain children, and ignoring the owner excludes all its edges. Ordinary `collider_id(owner, index)` addresses only declared Colliders.

Geometry, filters and materials are immutable after creation; destroy and recreate to replace them. `update_collider` and `set_collider_enabled` reject individual children; `set_body_enabled` toggles the group. `destroy_chain`, owner removal and Reset reclaim every child and mapping. Callback creation/destruction uses FIFO commands with copied definitions; pending creation has no `chain_state`. Invalid creation returns an invalid handle; allocation exceptions propagate while unfinished resources are cleaned up. Native IDs stay private.

## Submit a Kinematic Target Each Fixed Step

`set_target_transform(handle, PhysicsPose target, double fixed_delta_seconds)` computes velocity from the native pose when the command executes. `PhysicsParticipant` provides the same shortcut. Only Kinematic bodies, finite poses and valid positive steps are accepted; invalid requests return `false`. Fixed rotation tracks position only. Rotation takes the shortest path; identical or natively sleepy targets clear old velocities.

Submit a target in every `fixed_update(dt)`. This does not teleport, retain a persistent tracking controller, or guarantee exact arrival within one step. Stopping submissions leaves current velocity in effect; explicitly set velocity to stop. Callback parameters are copied and queued, rechecking live/enabled objects at execution. Velocity, angular velocity, teleports and targets follow FIFO order; rendering still reads `render_pose`. The physics center's `target_pose` page demonstrates a platform.

Filter and `collide_connected` follow the bundled Box2D solid-contact rules. They do not suppress native Sensor overlaps or change stateless queries. Use Collider filters or `CollisionQueryFilter` to exclude sensor/query targets.
