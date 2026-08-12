#define SDL_MAIN_HANDLED

#include "engine/application/lifecycle/application_event_boundary.h"
#include "engine/physics/physics_world.h"
#include "engine/tools/termination_manager.h"
#include "tests/support/test_assertions.h"

#include <algorithm>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

using elysia::tests::require;

namespace
{
class Object final
    : public elysia::core::GameObject
    , public elysia::physics::PhysicsBodyProvider
    , public elysia::physics::ColliderProvider
{
public:
    Object() : GameObject(elysia::core::DepthLayer::Character)
    {
        collider.shape = elysia::physics::AabbShape{{0, 0, 10, 10}};
    }
    elysia::physics::PhysicsBody* physics_body() noexcept override { return &body; }
    const elysia::physics::PhysicsBody* physics_body() const noexcept override { return &body; }
    std::span<elysia::physics::Collider> colliders() noexcept override { return {&collider, 1}; }
    std::span<const elysia::physics::Collider> colliders() const noexcept override
    { return {&collider, 1}; }

    elysia::physics::PhysicsBody body;
    elysia::physics::Collider collider;
};

class Listener final : public elysia::physics::ICollisionListener
{
public:
    void on_collision_event(const elysia::physics::CollisionEvent& event) override
    {
        phases.push_back(event.phase);
        events.push_back(event);
    }
    std::vector<elysia::physics::CollisionEventPhase> phases;
    std::vector<elysia::physics::CollisionEvent> events;
};

class ResetListener final : public elysia::physics::ICollisionListener
{
public:
    explicit ResetListener(elysia::physics::PhysicsWorld& world) : _world(&world) {}
    void on_collision_event(const elysia::physics::CollisionEvent&) override
    {
        ++events;
        _world->reset();
    }
    int events = 0;
private:
    elysia::physics::PhysicsWorld* _world = nullptr;
};

class ThrowingListener final : public elysia::physics::ICollisionListener
{
public:
    void on_collision_event(const elysia::physics::CollisionEvent&) override
    {
        throw std::runtime_error("physics listener failure");
    }
};

class TileWorld final : public elysia::physics::ITileCollisionWorld
{
public:
    elysia::core::Vector2 world_origin() const noexcept override { return {-16, -32}; }
    elysia::core::Vector2 tile_size() const noexcept override { return {16, 32}; }
    int columns() const noexcept override { return 3; }
    int rows() const noexcept override { return 2; }
    elysia::physics::TileOutOfBoundsPolicy out_of_bounds_policy() const noexcept override
    { return elysia::physics::TileOutOfBoundsPolicy::Empty; }
    elysia::physics::TileCollisionCell cell_at(
        elysia::physics::TileCoordinate coordinate) const noexcept override
    {
        elysia::physics::TileCollisionCell cell;
        if (coordinate == elysia::physics::TileCoordinate{1, 1})
            cell.type = elysia::physics::TileCollisionType::Block;
        return cell;
    }
};

class TileStripWorld final : public elysia::physics::ITileCollisionWorld
{
public:
    enum class Orientation { Floor, Wall };
    explicit TileStripWorld(Orientation orientation) : _orientation(orientation) {}
    elysia::core::Vector2 world_origin() const noexcept override { return {}; }
    elysia::core::Vector2 tile_size() const noexcept override { return {10, 10}; }
    int columns() const noexcept override { return 20; }
    int rows() const noexcept override { return 20; }
    elysia::physics::TileOutOfBoundsPolicy out_of_bounds_policy() const noexcept override
    { return elysia::physics::TileOutOfBoundsPolicy::Empty; }
    elysia::physics::TileCollisionCell cell_at(
        elysia::physics::TileCoordinate coordinate) const noexcept override
    {
        elysia::physics::TileCollisionCell cell;
        const bool solid = _orientation == Orientation::Floor
            ? coordinate.y == 1
            : coordinate.x == 1;
        if (solid)
        {
            cell.type = elysia::physics::TileCollisionType::Block;
            cell.material = {0, 0, 0};
        }
        return cell;
    }
private:
    Orientation _orientation;
};

[[nodiscard]] bool debug_snapshot_empty(
    const elysia::physics::PhysicsDebugSnapshot& snapshot)
{
    return snapshot.shapes.empty()
        && snapshot.broad_phase_pairs.empty()
        && snapshot.tile_candidates.empty()
        && snapshot.contacts.empty()
        && snapshot.velocities.empty();
}

void contacts_and_events()
{
    using namespace elysia::physics;
    PhysicsWorld world;
    Object dynamic;
    Object fixed;
    fixed.body.type = BodyType::Static;
    dynamic.set_position({0, 0});
    fixed.set_position({10, 0});
    const auto dynamic_handle = world.register_object(dynamic, &dynamic, &dynamic);
    require(dynamic_handle.is_valid()
            && world.register_object(fixed, &fixed, &fixed).is_valid(),
        "Runtime contact probes must register");
    Listener listener;
    require(world.add_listener(listener), "Core listeners must register");
    require(world.advance(1.0 / 60.0) == 1
            && listener.phases == std::vector{CollisionEventPhase::Begin},
        "Touching colliders must emit Begin");
    require(world.contact_state(dynamic_handle).wall_right,
        "A +X support normal must aggregate as the right wall");
    require(world.advance(1.0 / 60.0) == 1
            && listener.phases.back() == CollisionEventPhase::Stay,
        "Persistent contacts must emit Stay");
    dynamic.set_position({-100, 0});
    require(world.advance(1.0 / 60.0) == 1
            && listener.phases.back() == CollisionEventPhase::End,
        "Natural separation must emit End");

    dynamic.set_position({0, 0});
    (void)world.advance(1.0 / 60.0);
    const auto event_count = listener.events.size();
    require(world.teleport_object(dynamic_handle, {-100, 0}),
        "Teleport must accept registered finite positions");
    (void)world.advance(1.0 / 60.0);
    require(listener.events.size() == event_count,
        "Teleport must clear related contacts silently");
}

void reset_during_event_batch()
{
    using namespace elysia::physics;
    PhysicsWorldConfig config;
    config.fixed_delta_seconds = 0.1;
    PhysicsWorld world(config);
    Object first;
    Object second;
    second.body.type = BodyType::Static;
    second.set_position({10, 0});
    require(world.register_object(first, &first, &first).is_valid()
            && world.register_object(second, &second, &second).is_valid(),
        "Reset callback probes must register");
    ResetListener resetter(world);
    Listener observer;
    require(world.add_listener(resetter) && world.add_listener(observer),
        "Reset callback listeners must register");
    require(world.advance(0.3) == 1
            && resetter.events == 1 && observer.events.size() == 1,
        "A callback reset must finish the current listener batch and stop later fixed steps");
    require(world.registered_object_count() == 0
            && first.collider.id == InvalidColliderId
            && second.collider.id == InvalidColliderId
            && world.accumulator_seconds() == 0.0,
        "A deferred reset must clear registrations and accumulated time at the safe boundary");
}

void listener_exception_reaches_application_boundary()
{
    using namespace elysia;
    auto* termination = tools::TerminationManager::instance();
    termination->reset_for_testing();
    {
        physics::PhysicsWorld world;
        Object first;
        Object second;
        second.body.type = physics::BodyType::Static;
        second.set_position({10, 0});
        require(world.register_object(first, &first, &first).is_valid()
                && world.register_object(second, &second, &second).is_valid(),
            "Throwing listener probes must register");
        ThrowingListener listener;
        require(world.add_listener(listener),
            "Throwing listener must register");
        const bool completed = application::run_event_boundary("update", [&world]
        {
            (void)world.advance(1.0 / 60.0);
        });
        const auto info = termination->termination_info();
        require(!completed && info
                && info->reason == tools::TerminationReason::UnhandledException
                && info->category == "update"
                && info->message == "physics listener failure",
            "Listener exceptions must propagate to the Application fault boundary");
    }
    termination->reset_for_testing();
}

void ccd_queries_and_tiles()
{
    using namespace elysia::physics;
    PhysicsWorldConfig config;
    config.fixed_delta_seconds = 0.02;
    PhysicsWorld world(config);
    Object mover;
    Object wall;
    mover.collider.shape = AabbShape{{0, 0, 2, 2}};
    mover.collider.detection_mode = CollisionDetectionMode::Continuous;
    mover.body.velocity = {1000, 0};
    wall.collider.shape = AabbShape{{0, 0, 2, 10}};
    wall.body.type = BodyType::Static;
    wall.set_position({10, -4});
    require(world.register_object(mover, &mover, &mover).is_valid()
            && world.register_object(wall, &wall, &wall).is_valid(),
        "CCD probes must register");
    (void)world.advance(0.02);
    require(mover.position().x <= 8.01f
            && mover.body.velocity.x <= 0.01f
            && world.last_step_stats().ccd_hits > 0,
        "Swept AABB must stop a high-speed body at the impact boundary");

    mover.collider.enabled = false;
    RayCastQuery ray{{-5, 0}, {1, 0}, 30.0f};
    const auto hit = world.raycast(ray);
    require(hit && hit->target.kind == CollisionTargetKind::Collider
            && hit->distance >= 14.9f && hit->distance <= 15.1f,
        "Raycast must return the nearest collider hit");
    const auto segment = world.segment_cast({{-5, 0}, {30, 0}, {}});
    require(segment && segment->target == hit->target,
        "Segment cast must share nearest-hit semantics with raycast");

    PhysicsWorld tile_only;
    TileWorld tiles;
    require(tile_only.set_tile_world(tiles), "A valid non-square Tile world must bind");
    const auto tile_hit = tile_only.raycast({{8, -48}, {0, 1}, 100.0f});
    require(tile_hit && tile_hit->target
            == CollisionTarget::from_tile({1, 1}),
        "Tile DDA must honor negative origins and non-square cell sizes");

    PhysicsWorld corner_world(config);
    Object diagonal;
    Object vertical_wall;
    Object floor;
    diagonal.collider.shape = AabbShape{{0, 0, 2, 2}};
    diagonal.collider.detection_mode = CollisionDetectionMode::Continuous;
    diagonal.body.velocity = {1000, 1000};
    vertical_wall.collider.shape = AabbShape{{0, 0, 2, 30}};
    vertical_wall.body.type = BodyType::Static;
    vertical_wall.set_position({10, 0});
    floor.collider.shape = AabbShape{{0, 0, 30, 2}};
    floor.body.type = BodyType::Static;
    floor.set_position({0, 15});
    require(corner_world.register_object(diagonal, &diagonal, &diagonal).is_valid()
            && corner_world.register_object(
                vertical_wall, &vertical_wall, &vertical_wall).is_valid()
            && corner_world.register_object(floor, &floor, &floor).is_valid(),
        "Multiple-impact CCD probes must register");
    (void)corner_world.advance(0.02);
    require(diagonal.position().x <= 8.01f && diagonal.position().y <= 13.01f
            && diagonal.body.velocity.is_zero(0.01f)
            && corner_world.last_step_stats().ccd_iterations >= 2
            && corner_world.last_step_stats().ccd_iterations
                <= config.max_ccd_iterations,
        "CCD must continue through remaining time and stop at a second perpendicular impact");
}

void one_way_and_drop_through()
{
    using namespace elysia::physics;
    PhysicsWorldConfig config;
    config.fixed_delta_seconds = 0.02;
    PhysicsWorld world(config);
    Object actor;
    Object platform;
    actor.collider.shape = AabbShape{{0, 0, 4, 4}};
    actor.collider.detection_mode = CollisionDetectionMode::Continuous;
    platform.collider.shape = AabbShape{{0, 0, 20, 2}};
    platform.body.type = BodyType::Static;
    platform.collider.one_way = OneWayCollision{PassThroughDirection::Up, 0.01f};
    actor.set_position({4, 6});
    platform.set_position({0, 10});
    const auto actor_handle = world.register_object(actor, &actor, &actor);
    require(actor_handle.is_valid()
            && world.register_object(platform, &platform, &platform).is_valid(),
        "One-way probes must register");
    (void)world.advance(0.02);
    require(world.contact_state(actor_handle).grounded,
        "Standing on the solid side of an Up one-way platform must ground the actor");
    require(world.request_pass_through(
                actor.collider.id,
                CollisionTarget::from_collider(platform.collider.id)),
        "Drop-through must validate a current one-way support contact");
    actor.body.velocity = {0, 150};
    for (int i = 0; i < 4; ++i)
        (void)world.advance(0.02);
    require(actor.position().y > platform.position().y + 2.0f,
        "A drop-through ignore must remain active until complete separation");

    require(world.teleport_object(actor_handle, {4, 14}, TeleportVelocityMode::Clear),
        "The below-platform traversal probe must teleport safely");
    actor.body.velocity = {0, -300};
    (void)world.advance(0.02);
    require(actor.position().y < 10.0f,
        "PassThroughDirection::Up must allow travel from below toward -Y");

    PhysicsWorld overlap_world(config);
    Object overlap_actor;
    Object overlap_platform;
    overlap_platform.body.type = BodyType::Static;
    overlap_platform.collider.response = CollisionResponse::Overlap;
    overlap_platform.collider.one_way = OneWayCollision{
        PassThroughDirection::Up, 0.01f};
    overlap_platform.set_position({0, 8});
    require(overlap_world.register_object(
                overlap_actor, &overlap_actor, &overlap_actor).is_valid()
            && overlap_world.register_object(
                overlap_platform, &overlap_platform, &overlap_platform).is_valid(),
        "Overlap one-way probes must register");
    (void)overlap_world.advance(0.02);
    require(!overlap_world.request_pass_through(
                overlap_actor.collider.id,
                CollisionTarget::from_collider(overlap_platform.collider.id)),
        "Drop-through must reject a current OneWay contact classified as Overlap");
}

void materials_impulses_and_queries()
{
    using namespace elysia::physics;

    PhysicsWorldConfig friction_config;
    friction_config.fixed_delta_seconds = 0.01;
    friction_config.gravity = {0, 100};
    PhysicsWorld friction_world(friction_config);
    Object slider;
    Object ground;
    slider.body.velocity = {10, 0};
    slider.collider.material = {1.0f, 0.8f, 0.0f};
    ground.body.type = BodyType::Static;
    ground.collider.shape = AabbShape{{0, 0, 500, 20}};
    ground.collider.material = {1.0f, 0.8f, 0.0f};
    ground.set_position({-200, 10});
    require(friction_world.register_object(slider, &slider, &slider).is_valid()
            && friction_world.register_object(ground, &ground, &ground).is_valid(),
        "Friction probes must register");
    for (int i = 0; i < 80; ++i)
        (void)friction_world.advance(0.01);
    require(std::fabs(slider.body.velocity.x) < 0.05f,
        "Static and dynamic friction must bring a supported body to rest");

    PhysicsWorld zero_friction_world(friction_config);
    Object frictionless;
    Object smooth_ground;
    frictionless.body.velocity = {10, 0};
    frictionless.collider.material = {0, 0, 0};
    smooth_ground.body.type = BodyType::Static;
    smooth_ground.collider.shape = AabbShape{{0, 0, 500, 20}};
    smooth_ground.collider.material = {0, 0, 0};
    smooth_ground.set_position({-200, 10});
    require(zero_friction_world.register_object(
                frictionless, &frictionless, &frictionless).is_valid()
            && zero_friction_world.register_object(
                smooth_ground, &smooth_ground, &smooth_ground).is_valid(),
        "Zero-friction probes must register");
    for (int i = 0; i < 40; ++i)
        (void)zero_friction_world.advance(0.01);
    require(std::fabs(frictionless.body.velocity.x - 10.0f) < 0.05f,
        "Zero friction must preserve tangential velocity");

    PhysicsWorldConfig bounce_config;
    bounce_config.fixed_delta_seconds = 0.01;
    PhysicsWorld bounce_world(bounce_config);
    Object ball;
    Object bounce_ground;
    ball.body.velocity = {0, 10};
    ball.collider.material.restitution = 1.0f;
    bounce_ground.body.type = BodyType::Static;
    bounce_ground.collider.shape = AabbShape{{0, 0, 100, 10}};
    bounce_ground.collider.material.restitution = 1.0f;
    bounce_ground.set_position({-40, 10});
    require(bounce_world.register_object(ball, &ball, &ball).is_valid()
            && bounce_world.register_object(
                bounce_ground, &bounce_ground, &bounce_ground).is_valid(),
        "Restitution probes must register");
    (void)bounce_world.advance(0.01);
    require(ball.body.velocity.y < -9.0f,
        "Restitution must reverse a closing velocity above the threshold");

    PhysicsWorld platform_world(friction_config);
    Object passenger;
    Object platform;
    passenger.collider.material = {1, 1, 0};
    platform.body.type = BodyType::Kinematic;
    platform.body.velocity = {10, 0};
    platform.collider.shape = AabbShape{{0, 0, 60, 10}};
    platform.collider.material = {1, 1, 0};
    platform.set_position({-20, 10});
    require(platform_world.register_object(
                passenger, &passenger, &passenger).is_valid()
            && platform_world.register_object(platform, &platform, &platform).is_valid(),
        "Kinematic carry probes must register");
    (void)platform_world.advance(0.01);
    require(passenger.body.velocity.x > 0.0f
            && platform.body.velocity.x == 10.0f,
        "Relative-velocity friction must carry Dynamic bodies without pushing Kinematic bodies");

    PhysicsWorld inert_velocity_world;
    Object inert_dynamic;
    Object static_wall;
    static_wall.body.type = BodyType::Static;
    static_wall.body.velocity = {-100, 0};
    static_wall.set_position({10, 0});
    require(inert_velocity_world.register_object(
                inert_dynamic, &inert_dynamic, &inert_dynamic).is_valid()
            && inert_velocity_world.register_object(
                static_wall, &static_wall, &static_wall).is_valid(),
        "Static velocity probes must register");
    (void)inert_velocity_world.advance(1.0 / 60.0);
    require(inert_dynamic.body.velocity.is_zero(),
        "Static authored velocity must not create a phantom collision impulse");

    PhysicsWorld invalid_mass_world;
    Object unaffected;
    Object invalid_mass;
    invalid_mass.body.mass = 0.0f;
    invalid_mass.body.velocity = {-100, 0};
    invalid_mass.set_position({10, 0});
    require(invalid_mass_world.register_object(
                unaffected, &unaffected, &unaffected).is_valid()
            && invalid_mass_world.register_object(
                invalid_mass, &invalid_mass, &invalid_mass).is_valid(),
        "Invalid-mass velocity probes must register");
    (void)invalid_mass_world.advance(1.0 / 60.0);
    require(unaffected.body.velocity.is_zero(),
        "An invalid-mass Dynamic body must not create a phantom collision impulse");

    PhysicsWorld query_world;
    Object first;
    Object second;
    first.body.type = BodyType::Static;
    second.body.type = BodyType::Static;
    first.set_position({10, 0});
    second.collider.shape = CircleShape{{5, 5}, 5};
    second.collider.response = CollisionResponse::Overlap;
    second.set_position({25, 0});
    require(query_world.register_object(first, &first, &first).is_valid()
            && query_world.register_object(second, &second, &second).is_valid(),
        "Query probes must register");
    first.set_position({100, 0});

    std::vector<CollisionQueryHit> ray_hits;
    query_world.raycast_all({{0, 5}, {1, 0}, 50, {}}, ray_hits);
    require(ray_hits.size() == 2
            && ray_hits[0].target == CollisionTarget::from_collider(first.collider.id)
            && ray_hits[1].target == CollisionTarget::from_collider(second.collider.id)
            && ray_hits[0].response == CollisionResponse::Block
            && ray_hits[1].response == CollisionResponse::Overlap,
        "All-hit raycasts must sort mixed response hits by distance");
    const auto nearest = query_world.raycast({{0, 5}, {1, 0}, 50, {}});
    require(nearest && nearest->target == ray_hits.front().target,
        "Nearest raycast must reuse the first all-hit result");

    std::vector<CollisionOverlapQueryHit> overlaps;
    query_world.overlap_aabb({{8, 2, 6, 6}, {}}, overlaps);
    require(overlaps.size() == 1
            && overlaps.front().target
                == CollisionTarget::from_collider(first.collider.id),
        "AABB overlap must report current collider geometry");
    query_world.overlap_circle({{30, 5}, 3, {}}, overlaps);
    require(overlaps.size() == 1
            && overlaps.front().target
                == CollisionTarget::from_collider(second.collider.id),
        "Circle overlap must support Circle targets");
    const auto sweep = query_world.sweep_aabb({{0, 2, 2, 2}, {20, 0}, {}});
    require(sweep && sweep->target
            == CollisionTarget::from_collider(first.collider.id)
            && sweep->fraction > 0.39f && sweep->fraction < 0.41f,
        "AABB sweep must return the nearest AABB target and stable TOI");

    PhysicsWorld tile_query_world;
    TileWorld tiles;
    require(tile_query_world.set_tile_world(tiles),
        "Tile query probes must bind");
    tile_query_world.overlap_circle({{8, 16}, 4, {}}, overlaps);
    require(overlaps.size() == 1
            && overlaps.front().target == CollisionTarget::from_tile({1, 1}),
        "Overlap queries must honor negative Tile origins and non-square cells");
    const std::vector<elysia::core::Rect> touching_queries{
        {-2, 8, 2, 2},
        {16, 8, 2, 2},
        {4, -2, 2, 2},
        {4, 32, 2, 2}
    };
    for (const auto& bounds : touching_queries)
    {
        tile_query_world.overlap_aabb({bounds, {}}, overlaps);
        require(std::ranges::find(overlaps,
                    CollisionTarget::from_tile({1, 1}),
                    &CollisionOverlapQueryHit::target) != overlaps.end(),
            "AABB overlap must include a Tile touching any query boundary");
    }
    tile_query_world.overlap_circle({{20, 16}, 4, {}}, overlaps);
    require(std::ranges::find(overlaps,
                CollisionTarget::from_tile({1, 1}),
                &CollisionOverlapQueryHit::target) != overlaps.end(),
        "Circle overlap must include a tangent Tile");
    const auto tile_toi_one = tile_query_world.sweep_aabb(
        {{-4, 8, 2, 2}, {2, 0}, {}});
    require(tile_toi_one
            && tile_toi_one->target == CollisionTarget::from_tile({1, 1})
            && std::fabs(tile_toi_one->fraction - 1.0f) < 0.0001f,
        "AABB sweep must include a Tile first touched at TOI 1");

    PhysicsWorld circle_only_world;
    Object circle_only;
    circle_only.body.type = BodyType::Static;
    circle_only.collider.shape = CircleShape{{5, 5}, 5};
    require(circle_only_world.register_object(
                circle_only, &circle_only, &circle_only).is_valid(),
        "Zero sweep Circle exclusion probe must register");
    require(!circle_only_world.sweep_aabb({{0, 0, 10, 10}, {}, {}}),
        "Zero-displacement AABB sweep must still exclude Circle targets");

    tile_query_world.overlap_aabb(
        {{std::numeric_limits<float>::max() / 4.0f, 0, 16, 16}, {}}, overlaps);
    require(overlaps.empty(),
        "Unrepresentable Tile coordinates must be rejected without overflow");

    PhysicsWorldConfig bounded_tile_config;
    bounded_tile_config.max_tile_candidates_per_operation = 1;
    PhysicsWorld bounded_tile_world(bounded_tile_config);
    TileWorld bounded_tiles;
    Object oversized_tile_probe;
    oversized_tile_probe.collider.shape = AabbShape{{0, 0, 40, 10}};
    require(bounded_tile_world.set_tile_world(bounded_tiles)
            && bounded_tile_world.register_object(
                oversized_tile_probe,
                &oversized_tile_probe,
                &oversized_tile_probe).is_valid(),
        "Bounded Tile candidate probes must register");
    (void)bounded_tile_world.advance(1.0 / 60.0);
    require(bounded_tile_world.last_step_stats().rejected_tile_candidate_ranges == 1,
        "Oversized Tile candidate ranges must be rejected and diagnosed");
}

void tile_seams_and_basic_stacking()
{
    using namespace elysia::physics;

    PhysicsWorldConfig floor_config;
    floor_config.fixed_delta_seconds = 0.01;
    floor_config.gravity = {0, 100};
    PhysicsWorld floor_world(floor_config);
    TileStripWorld floor_tiles(TileStripWorld::Orientation::Floor);
    Object runner;
    runner.collider.shape = AabbShape{{0, 0, 8, 8}};
    runner.collider.detection_mode = CollisionDetectionMode::Continuous;
    runner.collider.material = {0, 0, 0};
    runner.body.velocity = {100, 0};
    runner.set_position({1, 2});
    require(floor_world.set_tile_world(floor_tiles)
            && floor_world.register_object(runner, &runner, &runner).is_valid(),
        "Floor seam probes must bind and register");
    for (int i = 0; i < 100; ++i)
        (void)floor_world.advance(0.01);
    require(runner.position().x > 100.0f
            && std::fabs(runner.body.velocity.x - 100.0f) < 0.1f,
        "Internal Tile faces must not catch a fast body crossing a continuous floor");

    PhysicsWorld wall_world(floor_config);
    TileStripWorld wall_tiles(TileStripWorld::Orientation::Wall);
    Object slider;
    slider.collider.shape = AabbShape{{0, 0, 8, 8}};
    slider.collider.detection_mode = CollisionDetectionMode::Continuous;
    slider.collider.material = {0, 0, 0};
    slider.body.gravity_scale = 0.0f;
    slider.body.velocity = {0, 100};
    slider.set_position({2, 1});
    require(wall_world.set_tile_world(wall_tiles)
            && wall_world.register_object(slider, &slider, &slider).is_valid(),
        "Wall seam probes must bind and register");
    for (int i = 0; i < 100; ++i)
        (void)wall_world.advance(0.01);
    require(slider.position().y > 100.0f
            && std::fabs(slider.body.velocity.y - 100.0f) < 0.1f,
        "Internal Tile faces must not catch a body sliding along a continuous wall");

    PhysicsWorldConfig stack_config;
    stack_config.fixed_delta_seconds = 1.0 / 120.0;
    stack_config.gravity = {0, 900};
    PhysicsWorld stack_world(stack_config);
    Object ground;
    Object bottom;
    Object middle;
    Object top;
    ground.body.type = BodyType::Static;
    ground.collider.shape = AabbShape{{0, 0, 100, 10}};
    ground.set_position({-45, 100});
    bottom.set_position({0, 90});
    middle.set_position({0, 80});
    top.set_position({0, 70});
    require(stack_world.register_object(ground, &ground, &ground).is_valid()
            && stack_world.register_object(bottom, &bottom, &bottom).is_valid()
            && stack_world.register_object(middle, &middle, &middle).is_valid()
            && stack_world.register_object(top, &top, &top).is_valid(),
        "Stack stability probes must register");
    for (int i = 0; i < 1200; ++i)
        (void)stack_world.advance(stack_config.fixed_delta_seconds);
    require(finite_vector(bottom.position())
            && finite_vector(middle.position())
            && finite_vector(top.position())
            && bottom.position().y < 90.1f
            && middle.position().y < 80.2f
            && top.position().y < 70.3f
            && bottom.body.velocity.length() < 1.0f
            && middle.body.velocity.length() < 1.0f
            && top.body.velocity.length() < 1.0f,
        "A small resting stack must remain finite and resist persistent sinking");
}

void debug_capture_is_selective_and_diagnostic_only()
{
    using namespace elysia::physics;

    PhysicsWorld world;
    Object first;
    Object second;
    first.body.type = BodyType::Static;
    second.body.type = BodyType::Static;
    second.set_position({10, 0});
    require(world.register_object(first, &first, &first).is_valid()
            && world.register_object(second, &second, &second).is_valid(),
        "Debug capture probes must register");

    require(world.debug_capture() == PhysicsDebugCapture::None,
        "Physics debug capture must default to disabled");
    (void)world.advance(1.0 / 60.0);
    require(debug_snapshot_empty(world.debug_snapshot())
            && world.last_step_stats().broad_phase_proxies == 2
            && world.last_step_stats().contacts == 1,
        "disabled capture must keep simulation statistics while producing no snapshot");
    const PhysicsStepStats baseline = world.last_step_stats();

    world.set_debug_capture(PhysicsDebugCapture::Shapes);
    (void)world.advance(1.0 / 60.0);
    require(world.debug_snapshot().shapes.size() == 2
            && world.debug_snapshot().broad_phase_pairs.empty()
            && world.debug_snapshot().contacts.empty()
            && world.debug_snapshot().velocities.empty(),
        "shape capture must not copy unrelated diagnostics");

    world.set_debug_capture(PhysicsDebugCapture::BroadPhase);
    require(debug_snapshot_empty(world.debug_snapshot()),
        "changing capture categories must clear the previous snapshot immediately");
    (void)world.advance(1.0 / 60.0);
    require(world.debug_snapshot().shapes.size() == 2
            && world.debug_snapshot().broad_phase_pairs.size() == 1
            && world.debug_snapshot().contacts.empty()
            && world.last_step_stats().broad_phase_proxies
                == baseline.broad_phase_proxies
            && world.last_step_stats().broad_phase_pairs
                == baseline.broad_phase_pairs
            && world.last_step_stats().contacts == baseline.contacts,
        "broad-phase capture must include required bounds without changing physics results");

    world.set_debug_capture(PhysicsDebugCapture::Contacts);
    (void)world.advance(1.0 / 60.0);
    require(world.debug_snapshot().shapes.empty()
            && world.debug_snapshot().broad_phase_pairs.empty()
            && world.debug_snapshot().contacts.size() == 1,
        "contact capture must copy only current contacts");

    world.set_debug_capture(PhysicsDebugCapture::None);
    require(debug_snapshot_empty(world.debug_snapshot()),
        "disabling capture must remove stale diagnostics immediately");

    PhysicsWorld velocity_world;
    Object moving;
    moving.body.velocity = {0, 20};
    require(velocity_world.register_object(moving, &moving, &moving).is_valid(),
        "Velocity capture probe must register");
    velocity_world.set_debug_capture(PhysicsDebugCapture::Velocities);
    (void)velocity_world.advance(1.0 / 60.0);
    require(velocity_world.debug_snapshot().velocities.size() == 1
            && velocity_world.debug_snapshot().shapes.empty()
            && velocity_world.debug_snapshot().contacts.empty(),
        "velocity capture must not require other diagnostic categories");
}
}

int main()
{
    contacts_and_events();
    reset_during_event_batch();
    listener_exception_reaches_application_boundary();
    ccd_queries_and_tiles();
    one_way_and_drop_through();
    materials_impulses_and_queries();
    tile_seams_and_basic_stacking();
    debug_capture_is_selective_and_diagnostic_only();
    std::cout << "physics runtime tests passed\n";
    return 0;
}
