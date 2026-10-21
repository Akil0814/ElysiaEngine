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
        _colliders[0].shape = elysia::physics::AabbShape{
            elysia::core::Rect{0, 0, 32, 32}};
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

Registration copies `BodyDefinition`. Static suits ground, Kinematic explicitly driven objects, Dynamic forces/gravity. Actors generally disable rotation. Density can calculate mass, or ExplicitMass supplies positive mass. Editing the original definition does not modify registered bodies; use mutation APIs.

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

`Collider` supports `AabbShape{ Rect local_rect }` and `CircleShape{ Vector2 local_center, float radius }`. Bodies may have multiple colliders; `collider_id(handle, index)` / `PhysicsParticipant::physics_collider(index)` returns stable IDs.

Local shapes use the body origin corresponding to object position. This fragment belongs in construction code including `engine/physics/physics_world.h`. Bits 1, 2, 4 are game-assigned and unrelated to `DepthLayer`.

```cpp
elysia::physics::Collider body_collider{
    .shape = elysia::physics::AabbShape{{-16.0f, -28.0f, 32.0f, 56.0f}},
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

Queries apply `CollisionFilter` and ignore disabled/`Ignore` shapes. `raycast`, `segment_cast`, `sweep_aabb` return nearest optional hits. `*_all` writes caller vectors sorted by `distance` and deduplicated by `target`.

| Need | Structure/interface | Result |
| --- | --- | --- |
| Laser/line of sight | `RayCastQuery` + `raycast` / `raycast_all` | `CollisionQueryHit` |
| Start/end line | `SegmentCastQuery` + `segment_cast` / `segment_cast_all` | `CollisionQueryHit` |
| Rectangular region | `AabbOverlapQuery` + `overlap_aabb` | `CollisionOverlapQueryHit` |
| Circular region | `CircleOverlapQuery` + `overlap_circle` | `CollisionOverlapQueryHit` |
| Fast bullet/actor box prediction | `AabbSweepQuery` + `sweep_aabb` | Nearest `CollisionQueryHit` |

`CollisionQueryHit` contains `target` (`Collider` or Tile), `point`, `normal`, `distance`, 0–1 `fraction`, and `response`. Queries create no collision events, damage, or `response`; game rules explicitly decide fast-attack hit timing.

## TileMap Collision

Implement `ITileCollisionWorld` with origin, cell size, dimensions, bounds policy, and `cell_at(TileCoordinate)`. `TileCollisionCell::type` is `Empty`/`Block`/`Overlap`/`OneWay`, with independent filter, material, tag, and rules.

After entry with valid map data, `physics_world().set_tile_world(map)` and check `bool`. After edits, `update_tiles(begin, end)` with inclusive bounds and check results. Before exit/map release call `clear_tile_world(map)` with the same instance.

TileMap is borrowed and must outlive binding. `TileOutOfBoundsPolicy::Block` creates four perimeter walls; `Empty` none. Adjacent solid cells with identical filters suppress internal faces; exposed faces collide. Slopes, partial cells, chunk streaming, and merged contours are absent.

## Joints

`create_distance_joint(const DistanceJointDefinition&)` creates fixed-`distance`/`spring` joints; `create_revolute_joint(const RevoluteJointDefinition&)` hinges with optional limits/motor. Successful creation returns `JointHandle`; `joint_state(handle)` reads anchors/reaction force/torque; `destroy_joint(handle)` removes.

Both definitions use local anchors and default `collide_connected=false`. Distance settings include `spring`/`frequency_hz`/`damping_ratio`. Revolute settings include `enable_limit`/`lower_angle`/`upper_angle`/`enable_motor`/`motor_speed`/`max_motor_torque`.

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

After both bodies register, create `DistanceJointDefinition{.first = a, .second = b, .length = 100.0f}`, pass to create_distance_joint, and check is_valid(). Destroy when finished and still check optional `joint_state` afterward. `Joints` cannot connect different worlds.

`TileCollisionCell` tag is std::string_view; both map and tag storage must span binding. World reset clears registration; do not reset independently while scene objects depend on it. Recreate can reset a whole level.

Common misuse includes double registration, `set_position` teleports, per-render repetition of fixed-step impulses, and undetached map/listener borrows.

See [interface reference](../../reference/physics/README.md) for the complete contract.

See [tile map guide](tilemap-collision.md) for binding, dirty regions, and one-way operations.

## References

- [Physics API](../../../../engine/physics/physics_world.h), [query parameters](../../../../engine/physics/collision/collision_query.h)
- [Gameplay collision](../gameplay/collision.md), [object queries](object-query.md), [back to user guide](../README.md)
