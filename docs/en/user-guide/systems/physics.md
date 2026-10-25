# Physics Objects, Collisions, and Queries

For game developers integrating bodies, collisions, and spatial queries, first confirm units/scene capabilities, create objects, modify simulation through commands, and consume results in suitable phases.

Scenes create `PhysicsWorld` on demand: enable both fixed_step and physics. `GameplayScene` defaults fixed_step but still requires physics. Usually `GameObject` implements `PhysicsParticipant` and automatically registers/unregisters on addition/removal. Do not own Box2D objects directly or repeat scene simulation / finalize_frame. See [scenes](../scene/scene.md) for lifetime.

## Minimal Physics Object

Place this type in a game header; create/check `create_and_add_object<FallingBox>()` on entry. It simulates without drawing; add [object rendering](../scene/game-objects.md) or [physics debug drawing](../tools/debug-draw.md).

```cpp
#include "engine/core/game_object.h"
#include "engine/physics/contracts/physics_participant.h"
#include "engine/physics/contracts/physics_step_participant.h"

#include <array>

class FallingBox final : public elysia::core::GameObject,
                         public elysia::physics::PhysicsParticipant,
                         public elysia::physics::PhysicsStepParticipant
{
public:
    FallingBox() : GameObject(elysia::core::DepthLayer::Item)
    {
        set_world_rect({100, 100, 32, 32});
        // Shapes are body-local; world placement comes from the object's world_rect.
        _colliders[0].shape = elysia::physics::BoxShape::from_rect(
            elysia::core::Rect{0, 0, 32, 32});
    }

    elysia::physics::BodyDefinition body_definition() const override
    {
        elysia::physics::BodyDefinition body;
        body.type = elysia::physics::BodyType::Dynamic;
        body.fixed_rotation = true;
        return body;
    }

    std::span<const elysia::physics::Collider> collider_definitions() const override
    {
        return _colliders;
    }

    void fixed_update(double) override
    {
        if (!physics_world())
            return;
        // Fixed updates set velocity only; physics advances position without a second manual move.
        set_velocity_x(40.0f);
    }

private:
    std::array<elysia::physics::Collider, 1> _colliders{};
};
```

Registration copies `BodyDefinition`. Static suits ground, Kinematic explicitly driven objects, Dynamic forces/gravity. Actors generally disable rotation. `mass_properties` defaults to `DensityMass{}`. `ExplicitMass{positive_mass}` scales inertia with total mass, while `BodyMassData` independently specifies mass, center, and inertia. Editing the original definition does not modify registered bodies; use mutation APIs.

Default world gravity is zero. Step size/max steps belong to `elysia::scene::FixedStepConfig` fields `delta_seconds` / `max_steps_per_frame`; Box2D subdivisions, scale, and gravity belong to `PhysicsWorldConfig`. Compose them through `SceneRuntimeFeatures`, not a physics configuration directly passed to Scene.

This complete scene uses defaults of 1/60 `second`, max 8 steps/frame, 4 Box2D substeps, and 100 world units/meter with explicit downward gravity:

```cpp
#include "engine/scene/scene.h"

class ConfiguredPhysicsScene final : public elysia::scene::Scene
{
public:
    ConfiguredPhysicsScene()
        : Scene(elysia::scene::SceneRuntimeFeatures{
            .fixed_step = elysia::scene::FixedStepConfig{
                .delta_seconds = 1.0 / 60.0,
                .max_steps_per_frame = 8},
            .physics = elysia::physics::PhysicsWorldConfig{
                .sub_steps = 4,
                .units_per_meter = 100,
                .gravity = {0.0f, 980.0f}}})
    {
    }

    // This demonstrates configuration only; games add FallingBox in on_enter as needed.
    void on_enter(const elysia::scene::ScenePayload&) override
    {
    }

    void on_exit() override
    {
    }

    void on_reset() override
    {
    }
};
```

Y points downward, so positive Y gravity is down. Configuration is fixed at construction. World drawing additionally needs an explicit camera.

After creation, check `physics_state()` or `contains_object(handle)` to confirm registration; non-null `GameObject` alone is insufficient. Invalid handles cannot support dependent gameplay bindings.

## Fixed Steps and Operations

`PhysicsStepParticipant` fixed_update runs on actual simulation steps, zero/multiple per frame; do not repeat one-shot input on every catch-up step. See [time](time-and-timers.md) for global scaling/pause.

`PhysicsParticipant` provides `physics_handle()`, `physics_collider(index)`, `physics_state()`, `velocity()`, `set_velocity(...)`, `set_velocity_x(...)`, `set_velocity_y(...)`, and `update_physics_collider(index, definition)`.

For direct world operations, use below. Object, collider, and joint handles/IDs are distinct. `bool` mutations return `false` on failure; `body_state`, `render_pose`, `joint_state` return `std::nullopt` when absent. Also check invalid values from creation/lookup.

| Goal | Interface |
| --- | --- |
| Register/query/unregister | `register_object(owner, body, colliders)`, `unregister_object(handle)`, `object_handle(owner)`, `collider_id(handle, index)` |
| Simulation state | `body_state(handle)`; presentation uses `render_pose(handle)` |
| Linear/angular velocity | `set_velocity`, `set_velocity_x`, `set_velocity_y`, `set_angular_velocity` |
| Gravity, enablement, waking | `set_gravity_scale`, `set_body_enabled`, `set_awake` |
| Forces/impulses | `apply_force`, `apply_impulse`, `apply_torque`, `apply_angular_impulse`; optional `world_point` is a world application `point` |
| Teleport | `teleport_object(handle, position, mode)` or `set_transform(handle, pose, mode)`; `TeleportVelocityMode::Clear` clears velocity |

Units are EU (engine units): position/length EU, velocity EU/s, gravity EU/s², angles radians, mass kg. `PhysicsWorldConfig::units_per_meter` defaults 100, step `1.0 / 60.0` seconds. `render_pose` interpolates previous/current simulation poses; rules/saves/game logic read `body_state`. Ordinary `GameObject` position writes are not physics teleports.

## Colliders, Filtering, and Response

`Collider` supports `BoxShape`, `CircleShape`, `CapsuleShape`, `ConvexPolygonShape` and `SegmentShape`. All geometry uses body-local coordinates. Multiple colliders per body have stable IDs through `collider_id(handle, index)` or `PhysicsParticipant::physics_collider(index)`.

| Shape | Definition |
| --- | --- |
| Box | `BoxShape{local_center, half_extents, local_angle}` with positive half extents; `BoxShape::from_rect(rect)` creates an unrotated local box |
| Circle | `CircleShape{local_center, radius}` with a positive radius |
| Capsule | `CapsuleShape{local_first, local_second, radius}`; coincident endpoints behave as a circle; capsules below native length precision collapse to a midpoint circle |
| Convex polygon | `ConvexPolygonShape::local_vertices` holds at most 8 vertices; `vertex_count` is 3–8, ordered along the boundary in either winding |
| Segment | `SegmentShape{local_first, local_second}`, zero-thickness and two-sided, Static/Kinematic only |

Concave, self-intersecting, duplicate, collinear/degenerate and non-finite vertices are rejected. Concave decomposition is not automatic. Registration and queries share validation and unit conversion; geometry must also meet native precision and finite mass/inertia requirements. `update_collider` preserves `ColliderId`; invalid replacements preserve the original shape. Independent static convex polygons can form slopes; Tile cells remain rectangular.

Local shapes use the body origin corresponding to object position. This fragment belongs in construction code including `engine/physics/physics_world.h`. Bits 1, 2, 4 are game-assigned and unrelated to `DepthLayer`.

```cpp
elysia::physics::Collider body_collider{
    .shape = elysia::physics::BoxShape::from_rect({-16.0f, -28.0f, 32.0f, 56.0f}),
    .filter = {.category = std::uint32_t{1}, .mask = std::uint32_t{2} | std::uint32_t{4}},
    .response = elysia::physics::CollisionResponse::Block,
    .material = {.friction = 0.4f, .restitution = 0.0f},
    .tag = "player-body",
};
```

- `CollisionResponse::Block` produces solid contact/solving, `Overlap` events without blocking, `Ignore` no participation.
- `CollisionFilter` `category`/`mask` match both ways. Equal nonzero `group` takes priority: positive forces collision, negative ignores.
- `Collider::enabled` / `set_collider_enabled(id, false)` disables shapes; `update_collider(id, definition)` replaces the entire copied definition.
- `PhysicsMaterial` supplies `friction`/`restitution`. Default density is `SurfaceDensity` (kg/m²). Sensors do not contribute mass unless `sensor_contributes_mass = true`.
- `BodyDefinition::bullet = true` / `Collider::detection_mode = Continuous` support fast solid bodies, not automatic continuous sensor/HitBox hits. Use explicit sweeps below for fast attacks.

## Events, Grounding, and One-Way Platforms

Implement `ICollisionListener::on_collision_event(const CollisionEvent&)`, register via `PhysicsWorld::add_listener(listener)`. Events contain `CollisionEventPhase::{Begin, Stay, End}`, stable ordered `CollisionPair`, `CollisionManifold` normals/penetration/up to two points, and `normal`/tangent impulses.

In fixed_update, check physics_world(), then `contact_state`(`physics_handle()`).`grounded` before jumping. This is existing simulation data, not prediction of the pending step.

`contact_state(PhysicsObjectHandle)` merges `grounded`/`ceiling`/`wall_left`/`wall_right` across colliders, or accepts `CollisionTarget`. `collect_contacts(target, out_contacts)` reads current snapshots. Normals `point` from `CollisionPair::first` to `second`. Prefer `contact_state` for individual targets rather than manually reversing normals.

One-way rules use `Collider::one_way` / `TileCollisionCell::one_way`, with `OneWayCollision::pass_through` combining Up/`Down`/`Left`/`Right`. Drop-through is not velocity mutation: call `request_pass_through(actor_collider, target)` for every current support. It accepts one-way targets only, wakes actors, and ignores pairs until separation. Multi-cell support needs per-cell requests.

## Stateless Spatial Queries

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

## TileMap Collision

Implement `ITileCollisionWorld` with origin, cell size, dimensions, bounds policy, and `cell_at(TileCoordinate)`. `TileCollisionCell::type` is `Empty`/`Block`/`Overlap`/`OneWay`, with independent filter, material, tag, and rules.

After entry with valid map data, `physics_world().set_tile_world(map)` and check `bool`. After edits, `update_tiles(begin, end)` with inclusive bounds and check results. Before exit/map release call `clear_tile_world(map)` with the same instance.

TileMap is borrowed and must outlive binding. `TileOutOfBoundsPolicy::Block` creates four perimeter walls; `Empty` none. Adjacent solid cells with identical filters suppress internal faces; exposed faces collide. Slopes, partial cells, chunk streaming, and merged contours are absent.

## Eight Joint Types and Runtime Settings

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
## Gameplay Collision

`Contacts` do not automatically deduct health. See [gameplay collision](../gameplay/collision.md) for actors, teams, HitBox/HurtBox, and deduplication.

## Debugging, Statistics, and Lifetime

Select capture with `set_debug_capture(PhysicsDebugCapture::Shapes | PhysicsDebugCapture::Contacts)`, then read `debug_snapshot()`.

Supported captures: `Shapes`, `BroadPhase`, `Contacts`, `Velocities`, `Joints`, `All`. `submit_physics_debug_snapshot(snapshot, debug_draw)` forwards to engine diagnostics.

`PhysicsDebugShape` stores previous/current poses and `native_bounds`, the simulation-step native axis-aligned bounds, not a rotated outline or CCD path.

`PhysicsWorld::last_step_stats()` reports objects/colliders, awake bodies, joints, contacts, and duration. Executed/dropped steps/interpolation belong to scene `FixedStepStats` via `Scene::fixed_step_stats()`, null if disabled. There is no public `accumulator_seconds()` query. `PhysicsWorld::reset()` clears objects/map/joints/contacts/listeners without resetting the scene accumulator.

Use the owning scene's world, never cached native Box2D IDs. Maps/listeners are borrowed and must detach before release. Teleport through `teleport_object`/`set_transform`, not `set_position` plus inconsistent simulation. Rendering reads `render_pose` (ordinary objects may use `render_rect()`); rules read `body_state`.

## Listener, Query, and Cleanup Examples

Use this helper in business functions with a valid scene world. Results do not cause damage. Targets can be `Collider` or Tile; inspect type rather than casting to `GameObject`.

```cpp
#include "engine/physics/physics_world.h"

std::optional<elysia::physics::CollisionQueryHit> find_obstacle(
    elysia::physics::PhysicsWorld& world,
    elysia::core::Vector2 start, elysia::core::Vector2 end)
{
    // Query only, without events; callers handle misses.
    return world.segment_cast({.start = start, .end = end});
}
```

No hit (`nullopt`) is `normal`. Configure filters for relevant layers. Region vectors contain `target` identifiers without ownership.

Listeners derive `ICollisionListener`/implement `on_collision_event`. On entry add/check `add_listener(*listener)`; before exit/destruction remove. Worlds borrow listeners. Record pending actions during callbacks; destroy listeners/reorganize objects afterward. Event handles can invalidate after removal.

After both bodies register, create `DistanceJointDefinition{.first = a, .second = b, .settings = {.length = 100.0f}}`, pass to create_distance_joint, and check is_valid(). Destroy when finished and still check optional `joint_state` afterward. `Joints` cannot connect different worlds.

`TileCollisionCell` tag is std::string_view; both map and tag storage must span binding. World reset clears registration; do not reset independently while scene objects depend on it. Recreate can reset a whole level.

Common misuse includes double registration, `set_position` teleports, per-render repetition of fixed-step impulses, and undetached map/listener borrows.

See [interface reference](../../reference/physics/README.md) for the complete contract.

See [tile map guide](tilemap-collision.md) for binding, dirty regions, and one-way operations.

## References

- [Physics API](../../../../engine/physics/physics_world.h), [query parameters](../../../../engine/physics/collision/collision_query.h)
- [Gameplay collision](../gameplay/collision.md), [object queries](object-query.md), [back to user guide](../README.md)

## Shape Cast Example

```cpp
#include "engine/physics/physics_world.h"

std::optional<elysia::physics::CollisionQueryHit> cast_capsule(
    const elysia::physics::PhysicsWorld& world,
    elysia::physics::PhysicsObjectHandle actor,
    elysia::core::Vector2 position, elysia::core::Vector2 displacement)
{
    return world.shape_cast({
        .shape = elysia::physics::CapsuleShape{{0, -12}, {0, 12}, 8},
        .pose = {position, 0},
        .displacement = displacement,
        .filter = {.ignored_object = actor}});
}
```

## Adjusting a Joint at Runtime

The physics test center joint page can change lengths and reverse motors. Its control button enters continuous manual simulation; reset returns to automatic verification.

```cpp
#include "engine/physics/physics_world.h"

bool reverse_joint_motor(elysia::physics::PhysicsWorld& world,
                         elysia::physics::JointHandle handle)
{
    auto state = world.joint_state(handle);
    if (!state)
        return false;
    const auto* hinge = std::get_if<elysia::physics::RevoluteJointState>(&state->details);
    if (!hinge)
        return false;
    auto settings = hinge->settings;
    settings.motor_speed = -settings.motor_speed;
    return world.update_revolute_joint(handle, settings);
}
```

## Segment and Chain Terrain

`SegmentShape{local_first, local_second}` describes a two-sided, zero-thickness local edge on Static or Kinematic objects only. It uses normal Collider filtering, material, response, enablement and stable IDs, contributes no mass, and preserves the old shape on invalid updates. Edge-edge physical solving is not promised. Overlap/cast sources remain boxes, circles, capsules and convex polygons only.

For connected terrain, call `create_chain(owner, definition)` on a registered Static/Kinematic object. `ChainDefinition` copies and owns `local_vertices` and `tag`, uses one `filter` and `material`, and always represents solid terrain detectable by sensors. An open chain needs at least two actual vertices; endpoint ghosts extrapolate the end edges unless `ghost_before`/`ghost_after` are supplied. Ghosts generate no collision edges. A closed chain (`loop=true`) needs at least four vertices without repeating the first. Edges must be at least 0.005 meters after unit conversion. Repeated, degenerate, intersecting, non-finite or immediately reversing geometry is rejected. Input order is preserved.

The front normal of local edge `a→b` is `(dy, -dx)`, then rotated with its body. In Y-down coordinates, a horizontal left-to-right edge blocks its upper side. Debug drawing shows edges and front arrows. Chain physical contacts and rays are one-sided; `overlap_shape` and `shape_cast` are geometric and can also hit the back. Gameplay consuming casts for movement must choose how to handle this difference.

`chain_state(handle)` reports owner, open/closed mode, vertex count and segment count. `chain_collider(handle, index)` returns the ColliderId of an actual edge; `chain_segment_info(id)` resolves its Chain, owner and zero-based index. Queries/events still use `CollisionTarget::Collider`. Owner contact state includes Chain children, and ignoring the owner excludes all its edges. Ordinary `collider_id(owner, index)` addresses only declared Colliders.

Geometry, filters and materials are immutable after creation; destroy and recreate to replace them. `update_collider` and `set_collider_enabled` reject individual children; `set_body_enabled` toggles the group. `destroy_chain`, owner removal and Reset reclaim every child and mapping. Callback creation/destruction uses FIFO commands with copied definitions; pending creation has no `chain_state`. Invalid creation returns an invalid handle; allocation exceptions propagate while unfinished resources are cleaned up. Native IDs stay private.

## Submit a Kinematic Target Each Fixed Step

`set_target_transform(handle, PhysicsPose target, double fixed_delta_seconds)` computes velocity from the native pose when the command executes. `PhysicsParticipant` provides the same shortcut. Only Kinematic bodies, finite poses and valid positive steps are accepted; invalid requests return `false`. Fixed rotation tracks position only. Rotation takes the shortest path; identical or natively sleepy targets clear old velocities.

Submit a target in every `fixed_update(dt)`. This does not teleport, retain a persistent tracking controller, or guarantee exact arrival within one step. Stopping submissions leaves current velocity in effect; explicitly set velocity to stop. Callback parameters are copied and queued, rechecking live/enabled objects at execution. Velocity, angular velocity, teleports and targets follow FIFO order; rendering still reads `render_pose`. The physics center's `target_pose` page demonstrates a platform.

These helpers use registered objects to create terrain, command a platform and create a slider. Callers check invalid handles and bool results.

```cpp
#include "engine/physics/physics_world.h"

elysia::physics::ChainHandle add_terrain(
    elysia::physics::PhysicsWorld& world, elysia::physics::PhysicsObjectHandle owner)
{
    elysia::physics::ChainDefinition chain;
    chain.local_vertices = {{0, 0}, {200, 0}, {300, -50}};
    chain.tag = "terrain";
    return world.create_chain(owner, chain); // owner must be Static or Kinematic
}

bool command_platform(elysia::physics::PhysicsWorld& world,
    elysia::physics::PhysicsObjectHandle platform, double dt)
{
    return world.set_target_transform(platform, {{200, 100}, 0.15f}, dt);
}

elysia::physics::JointHandle add_slider(elysia::physics::PhysicsWorld& world,
    elysia::physics::PhysicsObjectHandle anchor, elysia::physics::PhysicsObjectHandle body)
{
    elysia::physics::PrismaticJointDefinition joint{anchor, body};
    joint.local_axis = {1, 0};
    joint.settings.enable_motor = true;
    joint.settings.motor_speed = 30;
    joint.settings.max_motor_force = 1000;
    return world.create_prismatic_joint(joint);
}
```

Physics details start off: F2 shows joint axes/limits, F3 Chain ghosts and F4 the step target, including no-ImGui builds. Typed snapshots hide a joint when either endpoint is disabled; target diagnostics do not create persistent tracking. See [debug drawing](../tools/debug-draw.md) for toggles and timing.

## Configure Mass and Inertia Independently

Use `BodyMassData` to change mass while retaining rotational response. The creation configuration below describes a centered 256 × 8 EU rectangle at 0.5 kg/m² and 32 EU/m: density mass is 1 kg and inertia about its center is approximately 5466.67 kg·EU². Mass can become 0.5 kg while retaining that inertia; `ExplicitMass{0.5f}` instead halves inertia proportionally.

```cpp
#include "engine/physics/physics_world.h"
namespace mass_example {
namespace ph = elysia::physics;
ph::BodyDefinition seesaw_body() {
    ph::BodyDefinition body;
    body.fixed_rotation = false;
    body.mass_properties = ph::BodyMassData{0.5f, {}, 5466.666667f};
    return body;
}
bool preserve_inertia_with_new_mass(ph::PhysicsWorld& world, ph::PhysicsObjectHandle handle) {
    const auto state = world.body_state(handle);
    if (!state) return false;
    return world.set_mass_properties(handle,
        ph::BodyMassData{0.5f, state->local_center, state->rotational_inertia});
}
bool restore_density(ph::PhysicsWorld& world, ph::PhysicsObjectHandle handle) {
    return world.set_mass_properties(handle, ph::DensityMass{});
}
}
```

Pass the returned definition through registration. The runtime example reads actual data before replacing the complete configuration. `body_state().velocity` is center-of-mass velocity; when the center changes the engine adjusts it to keep origin velocity continuous. Fixed rotation makes effective inertia zero. Copying that value does not recover the masked requested inertia; read the original configuration through `mass_properties` when needed.

Collider changes reapply the configuration. Submit `DensityMass{}` to restore automatic mass. See [physics units and state](../../reference/physics/units-and-state.md) for validation, deferred execution, and units, and the [integration receipt](../../integration/006-explicit-inertia-receipt.md) for game integration.
