#define SDL_MAIN_HANDLED

#include "engine/physics/physics_world.h"
#include "tests/support/test_assertions.h"

#include <iostream>
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
}
}

int main()
{
    contacts_and_events();
    ccd_queries_and_tiles();
    one_way_and_drop_through();
    std::cout << "physics runtime tests passed\n";
    return 0;
}
