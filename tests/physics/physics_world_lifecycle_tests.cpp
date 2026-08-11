#define SDL_MAIN_HANDLED

#include "engine/physics/physics_world.h"
#include "engine/scene/scene.h"
#include "tests/support/test_assertions.h"

#include <cmath>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <vector>

using elysia::tests::require;

namespace
{
class PhysicsProbeObject final
    : public elysia::core::GameObject
    , public elysia::physics::PhysicsBodyProvider
    , public elysia::physics::ColliderProvider
{
public:
    explicit PhysicsProbeObject(
        std::size_t collider_count = 1,
        elysia::physics::ColliderId* id_at_destruction = nullptr)
        : GameObject(elysia::core::DepthLayer::Character)
        , _colliders(collider_count)
        , _id_at_destruction(id_at_destruction)
    {
    }

    ~PhysicsProbeObject() override
    {
        if (_id_at_destruction)
        {
            *_id_at_destruction = _colliders.empty()
                ? elysia::physics::InvalidColliderId
                : _colliders.front().id;
        }
    }

    elysia::physics::PhysicsBody* physics_body() noexcept override
    {
        return &_body;
    }

    const elysia::physics::PhysicsBody* physics_body() const noexcept override
    {
        return &_body;
    }

    std::span<elysia::physics::Collider> colliders() noexcept override
    {
        return _colliders;
    }

    std::span<const elysia::physics::Collider> colliders() const noexcept override
    {
        return _colliders;
    }

private:
    elysia::physics::PhysicsBody _body;
    std::vector<elysia::physics::Collider> _colliders;
    elysia::physics::ColliderId* _id_at_destruction = nullptr;
};

class AlternateColliderProvider final : public elysia::physics::ColliderProvider
{
public:
    std::span<elysia::physics::Collider> colliders() noexcept override
    {
        return std::span<elysia::physics::Collider>(&collider, 1);
    }

    std::span<const elysia::physics::Collider> colliders() const noexcept override
    {
        return std::span<const elysia::physics::Collider>(&collider, 1);
    }

    elysia::physics::Collider collider;
};

class FakeTileWorld final : public elysia::physics::ITileCollisionWorld
{
public:
    elysia::core::Vector2 world_origin() const noexcept override { return origin; }
    elysia::core::Vector2 tile_size() const noexcept override { return size; }
    int columns() const noexcept override { return width; }
    int rows() const noexcept override { return height; }
    elysia::physics::TileOutOfBoundsPolicy out_of_bounds_policy() const noexcept override
    {
        return elysia::physics::TileOutOfBoundsPolicy::Block;
    }
    elysia::physics::TileCollisionCell cell_at(
        elysia::physics::TileCoordinate coordinate) const noexcept override
    {
        (void)coordinate;
        return {};
    }

    elysia::core::Vector2 origin{};
    elysia::core::Vector2 size{16.0f, 16.0f};
    int width = 4;
    int height = 4;
};

class ProbeListener final : public elysia::physics::ICollisionListener
{
};

class PhysicsProbeScene final : public elysia::scene::Scene
{
public:
    void on_enter(const elysia::scene::ScenePayload& payload) override { (void)payload; }
    void on_exit() override {}
    void reset() override {}

    elysia::physics::PhysicsWorld& world() noexcept { return physics_world(); }
};

void test_collision_targets()
{
    using namespace elysia::physics;

    const CollisionTarget invalid;
    const CollisionTarget collider = CollisionTarget::from_collider(7);
    const CollisionTarget tile = CollisionTarget::from_tile({2, 3});
    require(!invalid.is_valid(), "Default collision targets must be invalid");
    require(collider.is_valid() && collider.kind == CollisionTargetKind::Collider
            && collider.collider == 7,
        "Collider targets must preserve collider identity");
    require(tile.is_valid() && tile.kind == CollisionTargetKind::Tile
            && tile.tile == TileCoordinate{2, 3},
        "Tile targets must preserve tile coordinates");
    require(CollisionTarget::from_collider(InvalidColliderId) == invalid,
        "Invalid collider IDs must create invalid targets");
    require(CollisionTarget::from_tile({0, 1}) < CollisionTarget::from_tile({1, 1})
            && CollisionTarget::from_tile({5, 0}) < CollisionTarget::from_tile({0, 1}),
        "Tile targets must sort by row and then column");
}

void test_registration_and_id_lifecycle()
{
    using namespace elysia::physics;

    PhysicsProbeObject body_only(0);
    PhysicsProbeObject collider_only(1);
    PhysicsProbeObject first(2);
    PhysicsProbeObject second(1);
    PhysicsWorld world;

    const PhysicsObjectHandle body_only_handle =
        world.register_object(body_only, &body_only, nullptr);
    const PhysicsObjectHandle collider_only_handle =
        world.register_object(collider_only, nullptr, &collider_only);
    require(body_only_handle.is_valid() && collider_only_handle.is_valid(),
        "Body-only and collider-only objects must register");
    require(world.unregister_object(body_only_handle)
            && world.unregister_object(collider_only_handle),
        "Body-only and collider-only objects must unregister");

    const PhysicsObjectHandle first_handle = world.register_object(first, &first, &first);
    require(first_handle.is_valid(), "Body and collider providers must register");
    require(world.registered_object_count() == 1
            && world.registered_collider_count() == 2,
        "Registration counts must include every collider");
    const auto first_colliders = first.colliders();
    const PhysicsProbeObject& const_first = first;
    require(first_colliders.data() == const_first.colliders().data()
            && first_colliders.size() == const_first.colliders().size(),
        "Mutable and const collider spans must expose the same stable storage");
    require(first_colliders[0].id != InvalidColliderId
            && first_colliders[1].id == first_colliders[0].id + 1,
        "Collider IDs must be assigned monotonically in provider order");
    require(world.contains_object(first_handle)
            && world.contains_object(first)
            && world.contains_collider(first_colliders[0].id),
        "Committed registration indices must be queryable");
    require(world.register_object(first, &first, &first) == first_handle,
        "Identical owner/provider registration must be idempotent");

    AlternateColliderProvider alternate;
    require(!world.register_object(first, &first, &alternate).is_valid(),
        "Changing providers for an existing owner must be rejected");

    const ColliderId released_id = first_colliders[1].id;
    require(world.unregister_object(first_handle),
        "Valid handles must unregister");
    require(first.colliders()[0].id == InvalidColliderId
            && first.colliders()[1].id == InvalidColliderId,
        "Unregistration must clear provider collider IDs");
    require(!world.unregister_object(first_handle),
        "Unregistration must not accept stale handles");

    const PhysicsObjectHandle second_handle = world.register_object(second, &second, &second);
    require(second_handle.is_valid() && second.colliders()[0].id > released_id,
        "Object handles and collider IDs must not be reused");

    PhysicsProbeObject prefilled(2);
    prefilled.colliders()[1].id = 99;
    require(!world.register_object(prefilled, &prefilled, &prefilled).is_valid(),
        "Prefilled collider IDs must be rejected");
    require(prefilled.colliders()[0].id == InvalidColliderId
            && prefilled.colliders()[1].id == 99,
        "Failed validation must not partially assign IDs");

    world.reset();
    require(second.colliders()[0].id == InvalidColliderId
            && world.registered_object_count() == 0
            && world.registered_collider_count() == 0,
        "World reset must clear registrations and provider IDs");
}

void test_world_destruction_clears_ids()
{
    using namespace elysia::physics;

    PhysicsProbeObject object;
    {
        PhysicsWorld world;
        require(world.register_object(object, &object, &object).is_valid(),
            "The destruction probe must register");
        require(object.colliders()[0].id != InvalidColliderId,
            "Registered colliders must receive IDs");
    }
    require(object.colliders()[0].id == InvalidColliderId,
        "PhysicsWorld destruction must clear borrowed collider IDs");
}

void test_tile_listener_and_empty_fixed_steps()
{
    using namespace elysia::physics;

    PhysicsWorldConfig config;
    config.fixed_delta_seconds = 0.1;
    config.max_steps_per_advance = 2;
    PhysicsWorld world(config);
    FakeTileWorld first_tile_world;
    FakeTileWorld second_tile_world;
    ProbeListener listener;

    require(world.set_tile_world(first_tile_world)
            && world.set_tile_world(first_tile_world)
            && world.tile_world() == &first_tile_world,
        "Binding the same tile world must be idempotent");
    require(!world.set_tile_world(second_tile_world)
            && !world.clear_tile_world(second_tile_world),
        "Conflicting tile world identities must be rejected");
    require(world.clear_tile_world(first_tile_world) && !world.tile_world(),
        "The active tile world must clear by identity");

    second_tile_world.size = {};
    require(!world.set_tile_world(second_tile_world),
        "Invalid tile sizes must be rejected");

    require(world.add_listener(listener) && world.add_listener(listener),
        "Listener registration must be idempotent");
    require(world.remove_listener(listener) && !world.remove_listener(listener),
        "Listener removal must reject unknown listeners");

    PhysicsProbeObject object;
    object.set_position({12.0f, 34.0f});
    object.physics_body()->velocity = {50.0f, -20.0f};
    require(world.register_object(object, &object, &object).is_valid(),
        "The fixed-step probe must register");
    const auto original_position = object.position();

    require(world.advance(0.05) == 0, "A partial fixed step must remain accumulated");
    require(world.advance(0.05) == 1, "Two partial advances must produce one fixed step");
    require(world.advance(0.45) == 2, "Advance must respect its fixed-step cap");
    require(std::fabs(world.accumulator_seconds() - 0.05) < 1e-9,
        "Advance must retain only the sub-step remainder after dropping excess work");
    require(world.advance(0.0) == 0
            && world.advance(-1.0) == 0
            && world.advance(std::numeric_limits<double>::quiet_NaN()) == 0,
        "Invalid frame deltas must not advance the world");
    require(world.advance(std::numeric_limits<double>::max()) == 2,
        "A huge finite delta must remain bounded by the fixed-step cap");
    require(object.position() != original_position
            && object.physics_body()->accumulated_force == elysia::core::Vector2{},
        "Fixed steps must integrate enabled Dynamic bodies and consume force");
    require(!world.raycast({}).has_value() && !world.segment_cast({}).has_value(),
        "Unimplemented world queries must safely return no hit");
}

void test_scene_registration_and_destruction_order()
{
    using namespace elysia::physics;

    ColliderId id_at_destruction = 999;
    {
        PhysicsProbeScene scene;
        PhysicsProbeObject* object = scene.create_and_add_object<PhysicsProbeObject>(
            1,
            &id_at_destruction);
        require(object && object->colliders()[0].id != InvalidColliderId,
            "Scene registration must assign collider IDs");
        require(scene.world().registered_object_count() == 1,
            "Scene-owned PhysicsWorld must observe registered providers");

        scene.pause();
        scene.on_update(0.25);
        require(scene.world().accumulator_seconds() == 0.0,
            "Paused scenes must not advance their PhysicsWorld");

        object->destroy();
        scene.resume();
        scene.on_update(0.0);
        require(scene.world().registered_object_count() == 0
                && id_at_destruction == InvalidColliderId,
            "Destroyed objects must unregister before their destructor runs");
    }

    id_at_destruction = 999;
    {
        PhysicsProbeScene scene;
        (void)scene.create_and_add_object<PhysicsProbeObject>(1, &id_at_destruction);
    }
    require(id_at_destruction == InvalidColliderId,
        "Scene PhysicsWorld must reset before owned GameObjects are destroyed");
}
}

int main()
{
    test_collision_targets();
    test_registration_and_id_lifecycle();
    test_world_destruction_clears_ids();
    test_tile_listener_and_empty_fixed_steps();
    test_scene_registration_and_destruction_order();
    std::cout << "physics world lifecycle tests passed\n";
    return 0;
}
